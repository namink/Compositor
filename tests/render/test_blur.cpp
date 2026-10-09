#include <gtest/gtest.h>

#include <cstdint>

#include "compositor/render/blur.hpp"

namespace {

using compositor::render::gaussian_blur;
using compositor::render::motion_blur;
using compositor::render::RgbaSurface;

RgbaSurface solid(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    RgbaSurface surface(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            surface.set(x, y, r, g, b, a);
        }
    }
    return surface;
}

std::uint8_t alpha_at(const RgbaSurface& surface, int x, int y) {
    return surface.data()[surface.offset(x, y) + 3];
}

std::uint8_t channel_at(const RgbaSurface& surface, int x, int y, int c) {
    return surface.data()[surface.offset(x, y) + static_cast<std::size_t>(c)];
}

TEST(BlurTest, GaussianLeavesAUniformInteriorAlone) {
    const RgbaSurface source = solid(21, 21, 100, 150, 200, 255);
    const RgbaSurface result = gaussian_blur(source, 2.0);
    EXPECT_NEAR(channel_at(result, 10, 10, 0), 100, 1);
    EXPECT_NEAR(channel_at(result, 10, 10, 1), 150, 1);
    EXPECT_NEAR(channel_at(result, 10, 10, 2), 200, 1);
    EXPECT_EQ(alpha_at(result, 10, 10), 255);
}

TEST(BlurTest, GaussianSpreadsASinglePixelSymmetrically) {
    RgbaSurface source(11, 11);
    source.set(5, 5, 255, 255, 255, 255);
    const RgbaSurface result = gaussian_blur(source, 1.5);
    const std::uint8_t center = alpha_at(result, 5, 5);
    EXPECT_LT(center, 255);
    EXPECT_GT(center, 0);
    EXPECT_EQ(alpha_at(result, 4, 5), alpha_at(result, 6, 5));
    EXPECT_EQ(alpha_at(result, 5, 4), alpha_at(result, 5, 6));
    EXPECT_GT(alpha_at(result, 4, 5), 0);
    EXPECT_GT(alpha_at(result, 3, 5), alpha_at(result, 5, 5) / 4);  // still falling off with distance
}

TEST(BlurTest, GaussianWithZeroSigmaIsANoOp) {
    const RgbaSurface source = solid(3, 3, 10, 20, 30, 255);
    const RgbaSurface result = gaussian_blur(source, 0.0);
    EXPECT_EQ(result.data()[0], source.data()[0]);
    EXPECT_EQ(result.offset(2, 2), source.offset(2, 2));
}

TEST(BlurTest, MotionSmearsAlongItsAngleOnly) {
    RgbaSurface source(11, 11);
    source.set(5, 5, 255, 255, 255, 255);
    const RgbaSurface result = motion_blur(source, 6.0, 0.0);  // horizontal
    EXPECT_LT(alpha_at(result, 5, 5), 255);
    EXPECT_GT(alpha_at(result, 4, 5), 0);
    EXPECT_GT(alpha_at(result, 6, 5), 0);
    // A horizontal streak leaves nothing above or below the source pixel.
    EXPECT_EQ(alpha_at(result, 5, 4), 0);
    EXPECT_EQ(alpha_at(result, 5, 6), 0);
}

TEST(BlurTest, MotionWithZeroDistanceIsANoOp) {
    RgbaSurface source(3, 3);
    source.set(1, 1, 5, 6, 7, 200);
    const RgbaSurface result = motion_blur(source, 0.0, 45.0);
    EXPECT_EQ(alpha_at(result, 1, 1), 200);
    EXPECT_EQ(channel_at(result, 1, 1, 0), 5);
}

}  // namespace
