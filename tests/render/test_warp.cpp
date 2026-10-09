#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "compositor/render/rgba_surface.hpp"
#include "compositor/render/warp.hpp"

namespace compositor::render {
namespace {

TEST(Warp, LiquifyMovesPixelsWithoutChangingAlpha) {
    RgbaSurface surface(24, 24);
    for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 24; ++x) {
            const auto value = static_cast<std::uint8_t>(x < 12 ? 40 : 220);
            surface.set(x, y, value, value, value, 255);
        }
    }
    const std::size_t bytes = static_cast<std::size_t>(24) * 24U * 4U;
    const std::vector<std::uint8_t> before(surface.data(), surface.data() + bytes);
    WarpStroke stroke(surface, WarpMode::liquify, 10.0, 0.7, 1.0);
    stroke.append(6.0, 6.0);
    stroke.append(18.0, 18.0);
    bool changed = false;
    for (std::size_t i = 0; i < before.size(); ++i) {
        if (before[i] != surface.data()[i]) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed);
    EXPECT_EQ(surface.data()[surface.offset(5, 5) + 3], 255);
}

TEST(Warp, SmudgeCarriesColorAcrossTheStroke) {
    RgbaSurface surface(24, 24);
    for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 24; ++x) {
            surface.set(x, y, 0, 0, 0, 255);
        }
    }
    for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 6; ++x) {
            surface.set(x, y, 255, 255, 255, 255);
        }
    }
    WarpStroke stroke(surface, WarpMode::smudge, 8.0, 0.5, 1.0);
    stroke.append(4.0, 12.0);
    for (int x = 4; x <= 20; ++x) {
        stroke.append(static_cast<double>(x), 12.0);
    }
    // White was dragged into the black half.
    EXPECT_GT(surface.data()[surface.offset(14, 12)], 0);
}

TEST(Warp, PerspectiveWithIdentityQuadKeepsTheImage) {
    RgbaSurface image(8, 8);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const auto value = static_cast<std::uint8_t>(x * 32);
            image.set(x, y, value, value, value, 255);
        }
    }
    Quad quad;
    quad.x[0] = 0.0;
    quad.y[0] = 0.0;
    quad.x[1] = 8.0;
    quad.y[1] = 0.0;
    quad.x[2] = 8.0;
    quad.y[2] = 8.0;
    quad.x[3] = 0.0;
    quad.y[3] = 8.0;
    int origin_x = 0;
    int origin_y = 0;
    RgbaSurface out;
    ASSERT_TRUE(warp_perspective(image, quad, false, false, origin_x, origin_y, out));
    EXPECT_EQ(out.width(), 8);
    EXPECT_EQ(out.height(), 8);
    EXPECT_EQ(out.data()[out.offset(4, 4) + 3], 255);
}

}  // namespace
}  // namespace compositor::render
