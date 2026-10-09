#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "compositor/io/psd_reader.hpp"
#include "compositor/render/rgba_surface.hpp"

namespace compositor::io::detail {

/// A bounds-checked reader over the file bytes.
class PsdCursor {
public:
    explicit PsdCursor(std::span<const std::uint8_t> data) : data_(data) {}

    [[nodiscard]] std::size_t offset() const { return offset_; }
    void seek(std::size_t position) { offset_ = position; }
    void skip(std::size_t count) {
        need(count);
        offset_ += count;
    }
    void need(std::size_t count) const {
        if (offset_ + count > data_.size()) {
            throw PsdError("The Photoshop file is truncated.");
        }
    }
    std::uint8_t u8() {
        need(1);
        return data_[offset_++];
    }
    std::uint16_t u16() {
        need(2);
        const std::uint16_t value = static_cast<std::uint16_t>((data_[offset_] << 8) | data_[offset_ + 1]);
        offset_ += 2;
        return value;
    }
    std::int16_t i16() { return static_cast<std::int16_t>(u16()); }
    std::uint32_t u32() {
        need(4);
        const std::uint32_t value = (static_cast<std::uint32_t>(data_[offset_]) << 24) |
                                    (static_cast<std::uint32_t>(data_[offset_ + 1]) << 16) |
                                    (static_cast<std::uint32_t>(data_[offset_ + 2]) << 8) |
                                    static_cast<std::uint32_t>(data_[offset_ + 3]);
        offset_ += 4;
        return value;
    }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
    std::uint64_t u64() {
        std::uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value = (value << 8) | u8();
        }
        return value;
    }
    std::span<const std::uint8_t> bytes(std::size_t count) {
        need(count);
        const std::span<const std::uint8_t> slice = data_.subspan(offset_, count);
        offset_ += count;
        return slice;
    }
    [[nodiscard]] std::string ascii(std::size_t count) {
        const std::span<const std::uint8_t> slice = bytes(count);
        return std::string(slice.begin(), slice.end());
    }
    [[nodiscard]] std::string latin1(std::size_t count) {
        const std::span<const std::uint8_t> slice = bytes(count);
        std::string text;
        text.reserve(count);
        for (const std::uint8_t byte : slice) {
            text.push_back(static_cast<char>(byte));
        }
        return text;
    }

private:
    std::span<const std::uint8_t> data_;
    std::size_t offset_ = 0;
};

struct PsdRawLayer {
    std::string name = "Layer";
    int top = 0, left = 0, bottom = 0, right = 0;
    std::uint8_t opacity = 255;
    std::uint8_t fill = 255;
    bool clipping = false;
    bool hidden = false;
    std::string blend_key = "norm";
    std::vector<std::pair<int, int>> channels;  // (id, length)
    int mask_top = 0, mask_left = 0, mask_bottom = 0, mask_right = 0;
    std::uint8_t mask_default = 255;
    bool mask_disabled = false;
    bool mask_linked = true;
    bool has_mask = false;
    std::optional<int> section;
    render::RgbaSurface image;
    std::vector<std::uint8_t> mask;
    int mask_width = 0;
    int mask_height = 0;
};

[[nodiscard]] std::string psd_utf16be_to_utf8(std::span<const std::uint8_t> data);

[[nodiscard]] bool psd_is_large_key(const std::string& key, bool is_psb);

[[nodiscard]] PsdRawLayer psd_read_record(PsdCursor& cursor, bool is_psb);

void psd_decode_layer_channels(PsdCursor& cursor, PsdRawLayer& layer, bool is_psb);

}  // namespace compositor::io::detail
