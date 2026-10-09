#include "compositor/io/psd_reader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "psd_internal.hpp"

namespace compositor::io {

bool looks_like_psd(std::span<const std::uint8_t> bytes) {
    return bytes.size() >= 4 && bytes[0] == '8' && bytes[1] == 'B' && bytes[2] == 'P' && bytes[3] == 'S';
}

PsdDocument read_psd(std::span<const std::uint8_t> bytes) {
    detail::PsdCursor cursor(bytes);
    if (cursor.ascii(4) != "8BPS") {
        throw PsdError("Not a Photoshop file.");
    }
    const std::uint16_t version = cursor.u16();
    if (version != 1 && version != 2) {
        throw PsdError("This Photoshop file uses a format version Compositor cannot read.");
    }
    const bool is_psb = version == 2;
    cursor.skip(6);
    cursor.u16();  // channel count
    const int canvas_height = static_cast<int>(cursor.u32());
    const int canvas_width = static_cast<int>(cursor.u32());
    const std::uint16_t depth = cursor.u16();
    const std::uint16_t mode = cursor.u16();
    if (depth != 8) {
        throw PsdError("Only 8-bit RGB Photoshop files can be imported.");
    }
    if (mode != 3) {
        throw PsdError("Only 8-bit RGB Photoshop files can be imported.");
    }
    PsdDocument document;
    document.width = canvas_width;
    document.height = canvas_height;
    cursor.skip(cursor.u32());  // color mode data
    const std::uint32_t resources_length = cursor.u32();
    const std::size_t resources_end = cursor.offset() + resources_length;
    while (cursor.offset() + 12 <= resources_end) {
        if (cursor.ascii(4) != "8BIM") {
            break;
        }
        const std::uint16_t id = cursor.u16();
        const int name_length = cursor.u8();
        cursor.skip(static_cast<std::size_t>(name_length));
        if ((name_length + 1) % 2 == 1) {
            cursor.skip(1);
        }
        const std::uint32_t length = cursor.u32();
        const std::size_t data_start = cursor.offset();
        if (id == 1005 && length >= 4) {
            document.resolution = static_cast<double>(cursor.u32()) / 65536.0;
            if (!(document.resolution >= 1.0)) {
                document.resolution = 72.0;
            }
            document.resolution = std::min(9600.0, std::max(1.0, document.resolution));
        }
        cursor.seek(data_start + length);
        if (length % 2 == 1) {
            cursor.skip(1);
        }
    }
    cursor.seek(resources_end);

    const std::uint64_t layer_section = is_psb ? cursor.u64() : cursor.u32();
    const std::size_t layer_section_end = cursor.offset() + static_cast<std::size_t>(layer_section);
    if (layer_section < 4) {
        return document;
    }
    if (is_psb) {
        cursor.u64();
    } else {
        cursor.u32();
    }
    const std::int16_t raw_count = cursor.i16();
    const int count = raw_count < 0 ? -static_cast<int>(raw_count) : static_cast<int>(raw_count);
    if (count > 10000) {
        throw PsdError("This Photoshop file has too many layers.");
    }
    std::vector<detail::PsdRawLayer> raw;
    raw.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        raw.push_back(detail::psd_read_record(cursor, is_psb));
    }
    for (detail::PsdRawLayer& layer : raw) {
        detail::psd_decode_layer_channels(cursor, layer, is_psb);
    }
    cursor.seek(layer_section_end);

    // Assemble into layers, resolving folder parentage: a type-3 divider opens a folder, its children
    // follow, and the folder record (type 1/2) closes it.
    std::vector<PsdLayer> layers;
    std::vector<int> group_stack;
    std::unordered_map<int, int> token_index;
    std::vector<std::pair<std::size_t, int>> pending_parent;
    int next_token = 0;
    for (detail::PsdRawLayer& layer : raw) {
        if (layer.section.has_value() && *layer.section == 3) {
            group_stack.push_back(next_token++);
            continue;
        }
        const bool is_group = layer.section.has_value() && (*layer.section == 1 || *layer.section == 2);
        const int token = is_group && !group_stack.empty() ? group_stack.back() : -1;
        if (is_group && !group_stack.empty()) {
            group_stack.pop_back();
        }
        PsdLayer record;
        record.name = layer.name.empty() ? "Layer" : layer.name;
        record.is_group = is_group;
        record.visible = !layer.hidden;
        record.clipping = layer.clipping;
        record.blend_key = layer.blend_key;
        record.left = layer.left;
        record.top = layer.top;
        record.width = std::max(0, layer.right - layer.left);
        record.height = std::max(0, layer.bottom - layer.top);
        record.opacity = (static_cast<double>(layer.opacity) / 255.0) * (static_cast<double>(layer.fill) / 255.0);
        if (!is_group) {
            record.image = std::move(layer.image);
        }
        if (layer.has_mask && !layer.mask.empty()) {
            record.has_mask = true;
            record.mask = std::move(layer.mask);
            record.mask_width = layer.mask_width;
            record.mask_height = layer.mask_height;
            record.mask_left = layer.mask_left;
            record.mask_top = layer.mask_top;
            record.mask_default = layer.mask_default;
            record.mask_enabled = !layer.mask_disabled;
            record.mask_linked = layer.mask_linked;
        }
        const int index = static_cast<int>(layers.size());
        pending_parent.emplace_back(static_cast<std::size_t>(index), group_stack.empty() ? -1 : group_stack.back());
        if (is_group && token >= 0) {
            token_index[token] = index;
        }
        layers.push_back(std::move(record));
    }
    for (const auto& [index, parent_token] : pending_parent) {
        if (parent_token < 0) {
            layers[index].parent = -1;
            continue;
        }
        const auto found = token_index.find(parent_token);
        if (found == token_index.end()) {
            throw PsdError("The Photoshop layer hierarchy is malformed.");
        }
        layers[index].parent = found->second;
    }
    document.layers = std::move(layers);
    return document;
}

}  // namespace compositor::io
