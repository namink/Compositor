#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "compositor/render/selection.hpp"
#include "support/fixtures.hpp"

namespace {

using compositor::render::clear_in_layer;
using compositor::render::combine;
using compositor::render::CombineMode;
using compositor::render::DocRect;
using compositor::render::fill_in_layer;
using compositor::render::from_polygon;
using compositor::render::from_rect;
using compositor::render::invert;
using compositor::render::magic_wand;
using compositor::render::RgbaSurface;
using compositor::render::Selection;

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

TEST(SelectionTest, RectClearErasesTheSelectedHalf) {
    RgbaSurface layer = solid(4, 4, 10, 20, 30, 255);
    const compositor::model::LayerTransform transform = comp_test::full_canvas_transform(4, 4);
    clear_in_layer(layer, transform, from_rect(DocRect{0, 0, 2, 4}, 4, 4));
    EXPECT_EQ(alpha_at(layer, 0, 0), 0);
    EXPECT_EQ(alpha_at(layer, 3, 3), 255);
}

TEST(SelectionTest, RectFillPaintsTheSelectedHalf) {
    RgbaSurface layer = solid(4, 4, 0, 0, 0, 0);
    const compositor::model::LayerTransform transform = comp_test::full_canvas_transform(4, 4);
    fill_in_layer(layer, transform, from_rect(DocRect{2, 0, 2, 4}, 4, 4), 1.0, 0.0, 0.0, 1.0);
    EXPECT_EQ(alpha_at(layer, 0, 0), 0);
    EXPECT_EQ(alpha_at(layer, 2, 0), 255);
    EXPECT_EQ(layer.data()[layer.offset(2, 0)], 255);
}

TEST(SelectionTest, PolygonRasterizesEvenOdd) {
    const std::vector<compositor::render::Point> triangle{{0.0, 0.0}, {8.0, 0.0}, {0.0, 8.0}};
    const Selection selection = from_polygon(triangle, 8, 8);
    EXPECT_EQ(selection.at(0, 0), 255);
    EXPECT_EQ(selection.at(6, 0), 255);
    EXPECT_EQ(selection.at(7, 7), 0);  // outside the hypotenuse
    EXPECT_EQ(selection.at(0, 7), 0);
    EXPECT_EQ(selection.min_x, 0);
    EXPECT_EQ(selection.max_x, 6);
}

TEST(SelectionTest, MagicWandSelectsTheContiguousColor) {
    RgbaSurface sample(4, 1);
    sample.set(0, 0, 255, 0, 0, 255);
    sample.set(1, 0, 255, 0, 0, 255);
    sample.set(2, 0, 0, 0, 255, 255);
    sample.set(3, 0, 0, 0, 255, 255);
    const compositor::model::LayerTransform transform = comp_test::full_canvas_transform(4, 1);
    const Selection selection = magic_wand(sample, transform, 4, 1, 0.5, 0.5, 0, true);
    EXPECT_EQ(selection.at(0, 0), 255);
    EXPECT_EQ(selection.at(1, 0), 255);
    EXPECT_EQ(selection.at(2, 0), 0);
    EXPECT_EQ(selection.at(3, 0), 0);
    EXPECT_EQ(selection.max_x, 1);
}

TEST(SelectionTest, InvertAndCombine) {
    const Selection left = from_rect(DocRect{0, 0, 2, 1}, 4, 1);
    const Selection inverted = invert(left);
    EXPECT_EQ(inverted.at(0, 0), 0);
    EXPECT_EQ(inverted.at(3, 0), 255);

    const Selection right = from_rect(DocRect{2, 0, 2, 1}, 4, 1);
    const Selection all = combine(left, right, CombineMode::add);
    EXPECT_EQ(all.at(0, 0), 255);
    EXPECT_EQ(all.at(3, 0), 255);
    const Selection none = combine(left, right, CombineMode::intersect);
    EXPECT_TRUE(none.empty());
}

TEST(SelectionTest, EmptySelectionIsEmpty) {
    EXPECT_TRUE(from_rect(DocRect{0, 0, 0, 0}, 4, 4).empty());
}

}  // namespace
