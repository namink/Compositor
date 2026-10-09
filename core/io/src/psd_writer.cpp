#include "compositor/io/psd_writer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace compositor::io {
namespace {

/// Big-endian byte sink, the layout Photoshop uses throughout.
class Writer {
public:
    void u8(std::uint8_t value) { bytes_.push_back(value); }
    void u16(std::uint16_t value) {
        bytes_.push_back(static_cast<std::uint8_t>(value >> 8));
        bytes_.push_back(static_cast<std::uint8_t>(value & 0xFF));
    }
    void i16(std::int16_t value) { u16(static_cast<std::uint16_t>(value)); }
    void u32(std::uint32_t value) {
        bytes_.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>(value & 0xFF));
    }
    void i32(std::int32_t value) { u32(static_cast<std::uint32_t>(value)); }
    void ascii(const char* text) {
        for (; *text != '\0'; ++text) {
            bytes_.push_back(static_cast<std::uint8_t>(*text));
        }
    }
    void raw(const std::vector<std::uint8_t>& data) { bytes_.insert(bytes_.end(), data.begin(), data.end()); }
    [[nodiscard]] std::size_t size() const { return bytes_.size(); }
    void pad_even() {
        if (bytes_.size() % 2 != 0) {
            bytes_.push_back(0);
        }
    }
    [[nodiscard]] std::vector<std::uint8_t> take() { return std::move(bytes_); }

private:
    std::vector<std::uint8_t> bytes_;
};

/// Straight (un-premultiplied) color for a premultiplied pixel, over white for the merged image.
[[nodiscard]] std::uint8_t straight(std::uint8_t component, std::uint8_t alpha) {
    if (alpha == 0) {
        return 255;
    }
    return static_cast<std::uint8_t>(std::min(255, (static_cast<int>(component) * 255 + alpha / 2) / alpha));
}

[[nodiscard]] std::uint8_t un_premul(std::uint8_t component, std::uint8_t alpha) {
    if (alpha == 0) {
        return 0;
    }
    return static_cast<std::uint8_t>(std::min(255, (static_cast<int>(component) * 255 + alpha / 2) / alpha));
}

std::vector<std::uint8_t> planar_channel(const render::RgbaSurface& image, int channel) {
    const std::size_t count = static_cast<std::size_t>(image.width()) * static_cast<std::size_t>(image.height());
    std::vector<std::uint8_t> plane(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t* pixel = image.data() + i * 4U;
        const std::uint8_t alpha = pixel[3];
        if (channel == 3) {
            plane[i] = alpha;
        } else {
            plane[i] = un_premul(pixel[static_cast<std::size_t>(channel)], alpha);
        }
    }
    return plane;
}

/// Photoshop blend keys for the blend modes we know; anything else writes as Normal.
[[nodiscard]] std::string blend_key(const std::string& project_key) {
    if (project_key.size() == 4) {
        return project_key;  // already a Photoshop key (from a PSD import)
    }
    return "norm";
}

void write_channel(Writer& writer, const std::vector<std::uint8_t>& plane) {
    writer.u16(0);  // RAW
    writer.raw(plane);
}

}  // namespace

std::vector<std::uint8_t> write_psd(const PsdDocument& document, const render::RgbaSurface& composite) {
    const std::uint16_t width = static_cast<std::uint16_t>(document.width);
    const std::uint16_t height = static_cast<std::uint16_t>(document.height);

    Writer header;
    header.ascii("8BPS");
    header.u16(1);
    for (int i = 0; i < 6; ++i) {
        header.u8(0);
    }
    header.u16(3);  // RGB
    header.u32(height);
    header.u32(width);
    header.u16(8);
    header.u16(3);  // RGB color mode
    header.u32(0);  // color mode data
    header.u32(0);  // image resources

    // Layer records first, so their channel-data lengths are known before the data blocks follow.
    Writer records;
    records.u16(static_cast<std::uint16_t>(document.layers.size()));
    std::vector<std::vector<std::uint8_t>> channel_planes;
    for (const PsdLayer& layer : document.layers) {
        const int left = static_cast<int>(std::lround(layer.left));
        const int top = static_cast<int>(std::lround(layer.top));
        const int layer_width = layer.image.empty() ? 0 : layer.image.width();
        const int layer_height = layer.image.empty() ? 0 : layer.image.height();
        const int right = left + layer_width;
        const int bottom = top + layer_height;
        const std::uint32_t plane_size =
            static_cast<std::uint32_t>(layer_width) * static_cast<std::uint32_t>(layer_height);
        records.i32(top);
        records.i32(left);
        records.i32(bottom);
        records.i32(right);
        records.u16(4);  // alpha, R, G, B
        records.i16(-1);
        records.u32(2U + plane_size);
        records.i16(0);
        records.u32(2U + plane_size);
        records.i16(1);
        records.u32(2U + plane_size);
        records.i16(2);
        records.u32(2U + plane_size);
        records.ascii("8BIM");
        std::string key = blend_key(layer.blend_key);
        if (key.size() < 4) {
            key = "norm";
        }
        key.resize(4);
        records.ascii(key.c_str());
        records.u8(static_cast<std::uint8_t>(std::clamp(std::lround(layer.opacity * 255.0), 0L, 255L)));
        records.u8(layer.clipping ? 1 : 0);
        records.u8(layer.visible ? 0 : 2);  // flags: bit 1 is "hidden"
        records.u8(0);                      // filler

        // Extra data: no mask, no blending ranges, then the Pascal name padded to a multiple of 4.
        Writer extra;
        extra.u32(0);  // layer mask data length
        extra.u32(0);  // blending ranges length
        const std::string name = layer.name.empty() ? "Layer" : layer.name.substr(0, 255);
        extra.u8(static_cast<std::uint8_t>(name.size()));
        extra.ascii(name.c_str());
        extra.pad_even();
        while (extra.size() % 4 != 0) {
            extra.u8(0);
        }
        records.u32(static_cast<std::uint32_t>(extra.size()));
        records.raw(extra.take());

        // Channel data blocks, in the same order as the channel info above: alpha, R, G, B.
        if (layer_width > 0 && layer_height > 0) {
            channel_planes.push_back(planar_channel(layer.image, 3));
            channel_planes.push_back(planar_channel(layer.image, 0));
            channel_planes.push_back(planar_channel(layer.image, 1));
            channel_planes.push_back(planar_channel(layer.image, 2));
        } else {
            for (int c = 0; c < 4; ++c) {
                channel_planes.emplace_back();
            }
        }
    }

    Writer layer_info = std::move(records);
    for (const std::vector<std::uint8_t>& plane : channel_planes) {
        write_channel(layer_info, plane);
    }
    layer_info.pad_even();

    Writer out = std::move(header);
    const std::size_t inner_size = layer_info.size();
    // Layer and Mask Information section: its own length, then the layer-info length, the layer info
    // (count, records, channel data), then the global layer mask length.
    out.u32(static_cast<std::uint32_t>(4 + inner_size + 4));
    out.u32(static_cast<std::uint32_t>(inner_size));
    out.raw(layer_info.take());
    out.u32(0);  // global layer mask info

    // Merged image data: RAW, planar R then G then B, over white.
    const std::size_t count =
        static_cast<std::size_t>(composite.width()) * static_cast<std::size_t>(composite.height());
    out.u16(0);  // RAW compression
    for (int channel = 0; channel < 3; ++channel) {
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint8_t* pixel = composite.data() + i * 4U;
            out.u8(straight(pixel[static_cast<std::size_t>(channel)], pixel[3]));
        }
    }
    return out.take();
}

}  // namespace compositor::io
