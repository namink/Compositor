#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "compositor/io/psd_reader.hpp"

namespace {

using compositor::io::looks_like_psd;
using compositor::io::read_psd;

void u16(std::vector<std::uint8_t>& out, int value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}
void u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}
void ascii(std::vector<std::uint8_t>& out, const char* text) {
    while (*text != '\0') {
        out.push_back(static_cast<std::uint8_t>(*text++));
    }
}

/// A 2x2, one-layer RGB PSD with a solid red opaque layer, built byte by byte.
std::vector<std::uint8_t> make_red_psd() {
    const int width = 2;
    const int height = 2;
    const int pixels = width * height;

    std::vector<std::uint8_t> header;
    ascii(header, "8BPS");
    u16(header, 1);  // version
    for (int i = 0; i < 6; ++i)
        header.push_back(0);  // reserved
    u16(header, 3);           // channels
    u32(header, static_cast<std::uint32_t>(height));
    u32(header, static_cast<std::uint32_t>(width));
    u16(header, 8);  // depth
    u16(header, 3);  // RGB

    std::vector<std::uint8_t> extra;
    u32(extra, 0);  // mask length
    u32(extra, 0);  // blending ranges
    const std::string name = "Red";
    extra.push_back(static_cast<std::uint8_t>(name.size()));
    ascii(extra, name.c_str());
    while (extra.size() % 4 != 0)
        extra.push_back(0);

    std::vector<std::uint8_t> record;
    u32(record, 0);                                   // top
    u32(record, 0);                                   // left
    u32(record, static_cast<std::uint32_t>(height));  // bottom
    u32(record, static_cast<std::uint32_t>(width));   // right
    u16(record, 4);                                   // channel count
    for (int id : {-1, 0, 1, 2}) {
        u16(record, id & 0xFFFF);
        u32(record, static_cast<std::uint32_t>(2 + pixels));  // compression + raw
    }
    ascii(record, "8BIM");
    ascii(record, "norm");
    record.push_back(255);  // opacity
    record.push_back(0);    // clipping
    record.push_back(0);    // flags
    record.push_back(0);    // filler
    u32(record, static_cast<std::uint32_t>(extra.size()));
    record.insert(record.end(), extra.begin(), extra.end());

    std::vector<std::uint8_t> channels;
    const int alpha[4] = {255, 255, 255, 255};
    const int red[4] = {255, 255, 255, 255};
    const int zero[4] = {0, 0, 0, 0};
    for (const int* plane : {alpha, red, zero, zero}) {
        u16(channels, 0);  // raw compression
        for (int i = 0; i < pixels; ++i)
            channels.push_back(static_cast<std::uint8_t>(plane[i]));
    }

    std::vector<std::uint8_t> layer_info;
    u16(layer_info, 1);  // layer count
    layer_info.insert(layer_info.end(), record.begin(), record.end());
    layer_info.insert(layer_info.end(), channels.begin(), channels.end());

    std::vector<std::uint8_t> layer_and_mask;
    u32(layer_and_mask, static_cast<std::uint32_t>(layer_info.size()));
    layer_and_mask.insert(layer_and_mask.end(), layer_info.begin(), layer_info.end());
    u32(layer_and_mask, 0);  // global layer mask info length

    std::vector<std::uint8_t> image_data;
    u16(image_data, 0);  // raw
    for (int i = 0; i < pixels; ++i)
        image_data.push_back(255);  // red plane
    for (int i = 0; i < pixels; ++i)
        image_data.push_back(0);
    for (int i = 0; i < pixels; ++i)
        image_data.push_back(0);

    std::vector<std::uint8_t> file = header;
    u32(file, 0);  // color mode data
    u32(file, 0);  // image resources
    u32(file, static_cast<std::uint32_t>(layer_and_mask.size()));
    file.insert(file.end(), layer_and_mask.begin(), layer_and_mask.end());
    file.insert(file.end(), image_data.begin(), image_data.end());
    return file;
}

TEST(PsdReaderTest, ReadsASingleLayer) {
    const std::vector<std::uint8_t> bytes = make_red_psd();
    ASSERT_TRUE(looks_like_psd(bytes));
    const compositor::io::PsdDocument document = read_psd(bytes);
    EXPECT_EQ(document.width, 2);
    EXPECT_EQ(document.height, 2);
    ASSERT_EQ(document.layers.size(), 1U);
    const compositor::io::PsdLayer& layer = document.layers[0];
    EXPECT_EQ(layer.name, "Red");
    EXPECT_FALSE(layer.is_group);
    EXPECT_TRUE(layer.visible);
    EXPECT_EQ(layer.parent, -1);
    ASSERT_EQ(layer.image.width(), 2);
    // Premultiplied opaque red.
    const std::uint8_t* pixel = layer.image.data();
    EXPECT_EQ(pixel[0], 255);
    EXPECT_EQ(pixel[1], 0);
    EXPECT_EQ(pixel[2], 0);
    EXPECT_EQ(pixel[3], 255);
}

TEST(PsdReaderTest, RejectsNonPsd) {
    const std::vector<std::uint8_t> junk{0x00, 0x01, 0x02, 0x03};
    EXPECT_FALSE(looks_like_psd(junk));
    EXPECT_THROW((void)read_psd(junk), compositor::io::PsdError);
}

}  // namespace
