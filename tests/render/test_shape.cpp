#include <gtest/gtest.h>

#include <cstdint>

#include "compositor/render/rgba_surface.hpp"
#include "compositor/render/shape.hpp"

namespace compositor::render {
namespace {

std::uint8_t alpha_at(const RgbaSurface& surface, int x, int y) {
    return surface.data()[surface.offset(x, y) + 3];
}

TEST(Shape, RectangleFillsItsBox) {
    ShapeStyle style;
    style.kind = ShapeKind::rectangle;
    style.red = 1.0F;
    style.blue = 0.0F;
    const RgbaSurface surface = draw_shape(style, 8, 8);
    EXPECT_EQ(alpha_at(surface, 0, 0), 255);
    EXPECT_EQ(alpha_at(surface, 4, 4), 255);
    EXPECT_EQ(alpha_at(surface, 7, 7), 255);
}

TEST(Shape, RoundedRectangleClearsItsCorners) {
    ShapeStyle style;
    style.kind = ShapeKind::rectangle;
    style.corner_radius = 8.0;  // clamped to half the shorter side
    const RgbaSurface surface = draw_shape(style, 16, 16);
    EXPECT_EQ(alpha_at(surface, 8, 8), 255);
    EXPECT_EQ(alpha_at(surface, 0, 0), 0);
    EXPECT_EQ(alpha_at(surface, 15, 15), 0);
}

TEST(Shape, EllipseClearsItsCorners) {
    ShapeStyle style;
    style.kind = ShapeKind::ellipse;
    const RgbaSurface surface = draw_shape(style, 16, 16);
    EXPECT_EQ(alpha_at(surface, 8, 8), 255);
    EXPECT_EQ(alpha_at(surface, 0, 0), 0);
}

TEST(Shape, LineStrokesBetweenItsEnds) {
    ShapeStyle style;
    style.kind = ShapeKind::line;
    style.line_width = 2.0;
    style.has_ends = true;
    style.start_x = 0.0;
    style.start_y = 0.0;
    style.end_x = 1.0;
    style.end_y = 1.0;
    const RgbaSurface surface = draw_shape(style, 16, 16);
    EXPECT_GT(alpha_at(surface, 8, 8), 0);
    EXPECT_EQ(alpha_at(surface, 15, 0), 0);  // a corner the line does not touch
}

}  // namespace
}  // namespace compositor::render
