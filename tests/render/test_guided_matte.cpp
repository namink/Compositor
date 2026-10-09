#include <gtest/gtest.h>

#include <vector>

#include "compositor/render/guided_matte.hpp"

namespace compositor::render {
namespace {

TEST(GuidedMatte, KeepsTheMaskInsideZeroOneAndLeavesAFlatGuideAlone) {
    const int width = 8;
    const int height = 8;
    std::vector<float> mask(width * height, 0.0F);
    std::vector<float> guide(width * height, 0.5F);
    for (int x = 2; x < 6; ++x) {
        mask[4 * width + x] = 1.0F;
    }
    const std::vector<float> refined = guided_filter(mask, guide, width, height, 2, 1e-4F);
    ASSERT_EQ(refined.size(), mask.size());
    for (float value : refined) {
        EXPECT_GE(value, 0.0F);
        EXPECT_LE(value, 1.0F);
    }
    // A flat guide has no edges to pull onto, so the mask's solid run stays above the empty area.
    EXPECT_GT(refined[4 * width + 3], refined[0]);
}

}  // namespace
}  // namespace compositor::render
