#include <gtest/gtest.h>

#include <cstdint>

#include "compositor/render/paint.hpp"
#include "compositor/render/rgba_surface.hpp"
#include "compositor/render/selection.hpp"

namespace compositor::render {
namespace {

TEST(ContentFill, FillsASmallSelectionFromSurroundings) {
    // A radius-2 match needs a fully known 5×5 window, so keep a margin around a small selection.
    RgbaSurface surface(12, 12);
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 12; ++x) {
            surface.set(x, y, 120, 120, 120, 255);
        }
    }
    Selection selection;
    selection.width = 12;
    selection.height = 12;
    selection.coverage.assign(144, 0);
    selection.coverage[8 * 12 + 8] = 255;
    selection.min_x = 8;
    selection.min_y = 8;
    selection.max_x = 8;
    selection.max_y = 8;
    EXPECT_TRUE(content_fill(surface, selection));
    EXPECT_EQ(surface.data()[surface.offset(8, 8) + 3], 255);
}

TEST(ContentFill, EmptySelectionIsRejected) {
    RgbaSurface surface(4, 4);
    surface.set(0, 0, 10, 10, 10, 255);
    Selection selection;
    selection.width = 4;
    selection.height = 4;
    selection.coverage.assign(16, 0);
    EXPECT_FALSE(content_fill(surface, selection));
}

}  // namespace
}  // namespace compositor::render
