#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "compositor/io/image_codec.hpp"
#include "compositor/render/rgba_surface.hpp"
#include "support/png_builder.hpp"

namespace {

using compositor::io::decode_image;
using compositor::io::decode_jpeg;
using compositor::io::decode_png;
using compositor::io::encode_jpeg;
using compositor::io::encode_png;
using compositor::io::ImageCodecError;
using compositor::render::RgbaSurface;

std::uint8_t premul(std::uint8_t channel, std::uint8_t alpha) {
    return static_cast<std::uint8_t>((static_cast<unsigned>(channel) * alpha + 127U) / 255U);
}

RgbaSurface solid(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    RgbaSurface surface(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            surface.set(x, y, premul(r, a), premul(g, a), premul(b, a), a);
        }
    }
    return surface;
}

void expect_pixel(const RgbaSurface& surface, int x, int y, int r, int g, int b, int a, int tolerance) {
    const std::uint8_t* p = surface.data() + surface.offset(x, y);
    EXPECT_NEAR(p[0], r, tolerance);
    EXPECT_NEAR(p[1], g, tolerance);
    EXPECT_NEAR(p[2], b, tolerance);
    EXPECT_NEAR(p[3], a, tolerance);
}

TEST(PngCodecTest, EncodesAndDecodesOpaquePixels) {
    const RgbaSurface surface = solid(3, 2, 10, 120, 240, 255);
    const std::vector<std::uint8_t> png = encode_png(surface);
    ASSERT_TRUE(compositor::io::looks_like_png(png));
    const RgbaSurface decoded = decode_png(png);
    ASSERT_EQ(decoded.width(), 3);
    ASSERT_EQ(decoded.height(), 2);
    expect_pixel(decoded, 0, 0, 10, 120, 240, 255, 0);
    expect_pixel(decoded, 2, 1, 10, 120, 240, 255, 0);
}

TEST(PngCodecTest, RoundTripsAlphaAsPremultiplied) {
    const RgbaSurface surface = solid(1, 1, 200, 100, 50, 128);
    const RgbaSurface decoded = decode_png(encode_png(surface));
    // Premultiplied channels survive the straight-alpha round trip within a level.
    expect_pixel(decoded, 0, 0, premul(200, 128), premul(100, 128), premul(50, 128), 128, 1);
}

TEST(PngCodecTest, DecodesGrayscaleMask) {
    const std::vector<std::uint8_t> gray = comp_test::make_png(4, 4, 0, 200);
    const RgbaSurface decoded = decode_png(gray);
    ASSERT_EQ(decoded.width(), 4);
    expect_pixel(decoded, 1, 1, 200, 200, 200, 255, 0);
}

TEST(PngCodecTest, RejectsNonPngBytes) {
    const std::vector<std::uint8_t> not_png{0x00, 0x01, 0x02, 0x03};
    EXPECT_THROW((void)decode_png(not_png), ImageCodecError);
}

TEST(JpegCodecTest, EncodesAndDecodesWithinTolerance) {
    const RgbaSurface surface = solid(8, 8, 128, 64, 32, 255);
    const std::vector<std::uint8_t> jpeg = encode_jpeg(surface, 95);
    ASSERT_TRUE(compositor::io::looks_like_jpeg(jpeg));
    const RgbaSurface decoded = decode_jpeg(jpeg);
    ASSERT_EQ(decoded.width(), 8);
    ASSERT_EQ(decoded.height(), 8);
    // JPEG is lossy; a flat color should still come back close.
    expect_pixel(decoded, 4, 4, 128, 64, 32, 255, 6);
}

TEST(DecodeDispatchTest, RecognizesFormatsAndRejectsOthers) {
    const RgbaSurface surface = solid(2, 2, 1, 2, 3, 255);
    const RgbaSurface from_png = decode_image(encode_png(surface));
    EXPECT_EQ(from_png.width(), 2);
    const RgbaSurface from_jpeg = decode_image(encode_jpeg(surface, 90));
    EXPECT_EQ(from_jpeg.width(), 2);
    EXPECT_THROW((void)decode_image(std::vector<std::uint8_t>{0x11, 0x22, 0x33}), ImageCodecError);
}

}  // namespace
