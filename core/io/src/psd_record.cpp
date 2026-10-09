#include <algorithm>
#include <cstddef>
#include <unordered_map>

#include "psd_channel.hpp"
#include "psd_internal.hpp"

namespace compositor::io::detail {

std::string psd_utf16be_to_utf8(std::span<const std::uint8_t> data) {
    if (data.size() < 4) {
        return {};
    }
    const std::uint32_t count = (static_cast<std::uint32_t>(data[0]) << 24) |
                                (static_cast<std::uint32_t>(data[1]) << 16) |
                                (static_cast<std::uint32_t>(data[2]) << 8) | data[3];
    std::string text;
    for (std::uint32_t i = 0; i < count && 4 + i * 2 + 1 < data.size(); ++i) {
        const std::uint16_t unit = static_cast<std::uint16_t>((data[4 + i * 2] << 8) | data[5 + i * 2]);
        if (unit == 0) {
            continue;
        }
        if (unit < 0x80) {
            text.push_back(static_cast<char>(unit));
        } else if (unit < 0x800) {
            text.push_back(static_cast<char>(0xC0 | (unit >> 6)));
            text.push_back(static_cast<char>(0x80 | (unit & 0x3F)));
        } else {
            text.push_back(static_cast<char>(0xE0 | (unit >> 12)));
            text.push_back(static_cast<char>(0x80 | ((unit >> 6) & 0x3F)));
            text.push_back(static_cast<char>(0x80 | (unit & 0x3F)));
        }
    }
    return text;
}

bool psd_is_large_key(const std::string& key, bool is_psb) {
    if (!is_psb) {
        return false;
    }
    static const std::array<const char*, 13> kKeys{"LMsk", "Lr16", "Lr32", "Layr", "Mt16", "Mt32", "Mtrn",
                                                   "Alph", "FMsk", "lnk2", "FEid", "FXid", "PxSD"};
    for (const char* candidate : kKeys) {
        if (key == candidate) {
            return true;
        }
    }
    return false;
}

PsdRawLayer psd_read_record(PsdCursor& cursor, bool is_psb) {
    PsdRawLayer layer;
    layer.top = cursor.i32();
    layer.left = cursor.i32();
    layer.bottom = cursor.i32();
    layer.right = cursor.i32();
    const int channel_count = cursor.u16();
    if (channel_count > 56) {
        throw PsdError("This Photoshop file has too many channels.");
    }
    for (int i = 0; i < channel_count; ++i) {
        const int id = cursor.i16();
        const std::uint64_t length = is_psb ? cursor.u64() : cursor.u32();
        layer.channels.emplace_back(id, static_cast<int>(length));
    }
    if (cursor.ascii(4) != "8BIM") {
        throw PsdError("The Photoshop layer record is malformed.");
    }
    layer.blend_key = cursor.ascii(4);
    layer.opacity = cursor.u8();
    layer.clipping = cursor.u8() != 0;
    const std::uint8_t flags = cursor.u8();
    layer.hidden = (flags & 2) != 0;
    cursor.skip(1);
    const std::uint32_t extra_length = cursor.u32();
    const std::size_t extra_end = cursor.offset() + extra_length;
    const std::uint32_t mask_length = cursor.u32();
    const std::size_t mask_end = cursor.offset() + mask_length;
    if (mask_length >= 20) {
        layer.has_mask = true;
        layer.mask_top = cursor.i32();
        layer.mask_left = cursor.i32();
        layer.mask_bottom = cursor.i32();
        layer.mask_right = cursor.i32();
        layer.mask_default = cursor.u8();
        const std::uint8_t mask_flags = cursor.u8();
        layer.mask_disabled = (mask_flags & 2) != 0;
        layer.mask_linked = (mask_flags & 1) == 0;
    }
    cursor.seek(mask_end);
    cursor.skip(cursor.u32());  // blending ranges
    const int name_count = cursor.u8();
    layer.name = cursor.latin1(static_cast<std::size_t>(name_count));
    const int name_pad = (4 - ((name_count + 1) % 4)) % 4;
    cursor.skip(static_cast<std::size_t>(name_pad));
    while (cursor.offset() + 12 <= extra_end) {
        const std::string signature = cursor.ascii(4);
        if (signature != "8BIM" && signature != "8B64") {
            break;
        }
        const std::string key = cursor.ascii(4);
        std::size_t length = 0;
        if (signature == "8B64" || psd_is_large_key(key, is_psb)) {
            if (cursor.offset() + 8 > extra_end) {
                break;
            }
            length = static_cast<std::size_t>(cursor.u64());
        } else {
            length = cursor.u32();
        }
        const std::span<const std::uint8_t> payload = cursor.bytes(length);
        if (length % 2 == 1) {
            cursor.skip(1);
        }
        if (key == "luni") {
            const std::string unicode = psd_utf16be_to_utf8(payload);
            if (!unicode.empty()) {
                layer.name = unicode;
            }
        } else if (key == "iOpa" && !payload.empty()) {
            layer.fill = payload[0];
        } else if ((key == "lsct" || key == "lsdk") && payload.size() >= 4) {
            layer.section = static_cast<int>((static_cast<std::uint32_t>(payload[0]) << 24) |
                                             (static_cast<std::uint32_t>(payload[1]) << 16) |
                                             (static_cast<std::uint32_t>(payload[2]) << 8) | payload[3]);
        }
    }
    cursor.seek(extra_end);
    return layer;
}

void psd_decode_layer_channels(PsdCursor& cursor, PsdRawLayer& layer, bool is_psb) {
    const int width = std::max(0, layer.right - layer.left);
    const int height = std::max(0, layer.bottom - layer.top);
    std::unordered_map<int, std::vector<std::uint8_t>> planes;
    for (const auto& [id, length] : layer.channels) {
        const std::size_t start = cursor.offset();
        if (id != -1 && id != 0 && id != 1 && id != 2 && id != -2) {
            cursor.seek(start + static_cast<std::size_t>(std::max(0, length)));
            continue;
        }
        if (length >= 2 && width > 0 && height > 0) {
            const int compression = cursor.u16();
            const std::span<const std::uint8_t> payload = cursor.bytes(static_cast<std::size_t>(length) - 2);
            if (id == -2) {
                const int mask_w = std::max(0, layer.mask_right - layer.mask_left);
                const int mask_h = std::max(0, layer.mask_bottom - layer.mask_top);
                if (mask_w > 0 && mask_h > 0) {
                    planes[id] = decode_channel(compression, mask_w, mask_h, payload, is_psb);
                }
            } else {
                planes[id] = decode_channel(compression, width, height, payload, is_psb);
            }
        }
        cursor.seek(start + static_cast<std::size_t>(std::max(0, length)));
    }
    const int mask_w = std::max(0, layer.mask_right - layer.mask_left);
    const int mask_h = std::max(0, layer.mask_bottom - layer.mask_top);
    const auto mask = planes.find(-2);
    if (layer.has_mask && mask_w > 0 && mask_h > 0 && mask != planes.end() &&
        mask->second.size() >= static_cast<std::size_t>(mask_w) * mask_h) {
        layer.mask = mask->second;
        layer.mask_width = mask_w;
        layer.mask_height = mask_h;
    }
    if (width <= 0 || height <= 0) {
        return;
    }
    const std::size_t count = static_cast<std::size_t>(width) * height;
    const std::vector<std::uint8_t> opaque(count, 255U);
    const std::vector<std::uint8_t> black(count, 0U);
    const std::vector<std::uint8_t>& red = planes.count(0) ? planes[0] : black;
    const std::vector<std::uint8_t>& green = planes.count(1) ? planes[1] : black;
    const std::vector<std::uint8_t>& blue = planes.count(2) ? planes[2] : black;
    const std::vector<std::uint8_t>& alpha = planes.count(-1) ? planes[-1] : opaque;
    if (red.size() < count || green.size() < count || blue.size() < count || alpha.size() < count) {
        throw PsdError("The Photoshop layer data is truncated.");
    }
    layer.image = render::RgbaSurface(width, height);
    std::uint8_t* out = layer.image.data();
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t a = alpha[i];
        const auto premul = [a](std::uint8_t c) {
            return static_cast<std::uint8_t>((static_cast<unsigned>(c) * a + 127U) / 255U);
        };
        out[i * 4U] = premul(red[i]);
        out[i * 4U + 1U] = premul(green[i]);
        out[i * 4U + 2U] = premul(blue[i]);
        out[i * 4U + 3U] = a;
    }
}

}  // namespace compositor::io::detail
