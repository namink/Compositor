#pragma once
#include <vector>

namespace compositor::render {

/// Guided filtering (He, Sun & Tang), ported from the macOS app's `GuidedMatte`: a coverage mask
/// pulled onto the edges of the image it came from, which recovers the fine detail a coarse mask cuts
/// through. Both `mask` and `guide` are `width`×`height` values in 0–1.
[[nodiscard]] std::vector<float> guided_filter(std::vector<float> mask, std::vector<float> guide, int width, int height,
                                               int radius, float epsilon);

}  // namespace compositor::render
