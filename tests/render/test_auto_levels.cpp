#include <gtest/gtest.h>

#include <cstdint>

#include "compositor/render/adjustment.hpp"
#include "compositor/render/rgba_surface.hpp"

namespace compositor::render {
namespace {

void set_pixel(RgbaSurface& surface, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    surface.set(x, y, r, g, b, a);
}

TEST(AutoLevels, ColorStretchesEachChannel) {
    RgbaSurface surface(256, 1);
    for (int x = 0; x < 256; ++x) {
        const auto value = static_cast<std::uint8_t>(x);
        set_pixel(surface, x, 0, value, value, value, 255);
    }
    const nlohmann::json levels = auto_levels_settings(surface, 1);
    ASSERT_TRUE(levels.contains("ranges"));
    const nlohmann::json& ranges = levels.at("ranges");
    ASSERT_EQ(ranges.size(), 4U);
    // The red channel range ends below 255: the layer's own pixels are stretched to fill 0..255.
    EXPECT_LT(ranges.at(1).value("black", -1.0), 255.0);
    EXPECT_GT(ranges.at(1).value("white", -1.0), 0.0);
}

TEST(AutoLevels, ContrastSharesOneInterval) {
    RgbaSurface surface(4, 1);
    set_pixel(surface, 0, 0, 10, 20, 30, 255);
    set_pixel(surface, 1, 0, 200, 180, 160, 255);
    set_pixel(surface, 2, 0, 120, 120, 120, 255);
    set_pixel(surface, 3, 0, 90, 90, 90, 255);
    const nlohmann::json levels = auto_levels_settings(surface, 0);
    const nlohmann::json& ranges = levels.at("ranges");
    ASSERT_EQ(ranges.size(), 4U);
    EXPECT_LT(ranges.at(0).value("black", -1.0), ranges.at(0).value("white", -1.0));
}

}  // namespace
}  // namespace compositor::render
