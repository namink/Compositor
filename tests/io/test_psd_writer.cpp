#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "compositor/io/psd_reader.hpp"
#include "compositor/io/psd_writer.hpp"
#include "compositor/render/rgba_surface.hpp"

namespace compositor::io {
namespace {

render::RgbaSurface fill(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    render::RgbaSurface surface(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            surface.set(x, y, r, g, b, a);
        }
    }
    return surface;
}

TEST(PsdWriter, RoundTripsFlatLayers) {
    PsdDocument document;
    document.width = 12;
    document.height = 10;

    PsdLayer bottom;
    bottom.name = "Bottom";
    bottom.left = 0;
    bottom.top = 0;
    bottom.image = fill(12, 10, 255, 0, 0, 255);
    PsdLayer top;
    top.name = "Top";
    top.left = 2;
    top.top = 1;
    top.opacity = 0.5;
    top.image = fill(5, 4, 0, 0, 255, 255);
    document.layers = {bottom, top};

    const render::RgbaSurface composite = fill(12, 10, 200, 100, 50, 255);
    const std::vector<std::uint8_t> bytes = write_psd(document, composite);
    ASSERT_TRUE(looks_like_psd(bytes)) << "The writer did not produce a PSD signature";

    const PsdDocument out = read_psd(bytes);
    EXPECT_EQ(out.width, 12);
    EXPECT_EQ(out.height, 10);
    ASSERT_EQ(out.layers.size(), 2U);
    EXPECT_EQ(out.layers[0].name, "Bottom");
    EXPECT_EQ(out.layers[1].name, "Top");
    EXPECT_EQ(out.layers[1].image.width(), 5);
    EXPECT_EQ(out.layers[1].image.height(), 4);
}

}  // namespace
}  // namespace compositor::io
