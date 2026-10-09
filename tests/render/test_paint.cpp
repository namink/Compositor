#include <gtest/gtest.h>

#include <cstdint>

#include "compositor/render/paint.hpp"
#include "compositor/render/placement.hpp"
#include "support/fixtures.hpp"

namespace {

using compositor::render::BrushDab;
using compositor::render::layer_pixel_at;
using compositor::render::RgbaSurface;
using compositor::render::stamp_dab;
using compositor::render::stroke_segment;

std::uint8_t alpha_at(const RgbaSurface& surface, int x, int y) {
    return surface.data()[surface.offset(x, y) + 3];
}

TEST(PaintTest, HardDabFillsItsCenter) {
    RgbaSurface surface(11, 11);
    BrushDab dab;
    dab.x = 5.5;
    dab.y = 5.5;
    dab.radius = 2.0;
    dab.hardness = 1.0;
    dab.opacity = 1.0;
    dab.red = 0.0;
    dab.green = 0.0;
    dab.blue = 0.0;
    stamp_dab(surface, dab);
    EXPECT_EQ(alpha_at(surface, 5, 5), 255);
    EXPECT_EQ(surface.data()[surface.offset(5, 5)], 0);  // black
    EXPECT_EQ(alpha_at(surface, 0, 0), 0);               // outside the dab
}

TEST(PaintTest, SoftDabFadesTowardTheEdge) {
    RgbaSurface surface(21, 21);
    BrushDab dab;
    dab.x = 10.5;
    dab.y = 10.5;
    dab.radius = 8.0;
    dab.hardness = 0.0;
    dab.opacity = 1.0;
    stamp_dab(surface, dab);
    const int center = alpha_at(surface, 10, 10);
    const int mid = alpha_at(surface, 14, 10);
    const int edge = alpha_at(surface, 17, 10);
    EXPECT_GT(center, 240);
    EXPECT_LT(mid, center);
    EXPECT_GT(mid, 0);
    EXPECT_LT(edge, mid);
}

TEST(PaintTest, EraseLowersCoverage) {
    RgbaSurface surface(11, 11);
    for (int y = 0; y < 11; ++y) {
        for (int x = 0; x < 11; ++x) {
            surface.set(x, y, 255, 255, 255, 255);
        }
    }
    BrushDab erase;
    erase.x = 5.5;
    erase.y = 5.5;
    erase.radius = 3.0;
    erase.hardness = 1.0;
    erase.opacity = 1.0;
    erase.erase = true;
    stamp_dab(surface, erase);
    EXPECT_EQ(alpha_at(surface, 5, 5), 0);
    EXPECT_EQ(alpha_at(surface, 0, 0), 255);
}

TEST(PaintTest, StrokePaintsAContinuousLine) {
    RgbaSurface surface(11, 11);
    BrushDab dab;
    dab.radius = 1.0;
    dab.hardness = 1.0;
    dab.opacity = 1.0;
    stroke_segment(surface, 2.5, 5.5, 8.5, 5.5, dab);
    for (int x = 2; x <= 8; ++x) {
        EXPECT_EQ(alpha_at(surface, x, 5), 255) << "at x=" << x;
    }
    EXPECT_EQ(alpha_at(surface, 5, 0), 0);
}

TEST(PaintTest, LayerPixelAtInvertsTheTransform) {
    const compositor::model::LayerTransform transform = comp_test::full_canvas_transform(10, 10);
    double px = 0.0;
    double py = 0.0;
    ASSERT_TRUE(layer_pixel_at(transform, 10, 10, 3.5, 4.5, px, py));
    EXPECT_DOUBLE_EQ(px, 3.5);
    EXPECT_DOUBLE_EQ(py, 4.5);
}

}  // namespace
