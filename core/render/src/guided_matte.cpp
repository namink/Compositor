#include "compositor/render/guided_matte.hpp"

#include <algorithm>
#include <cstddef>

// Ported from the macOS app's `GuidedMatte`: a box mean done as two running-sum passes (the cost does
// not grow with the radius), then the guided-filter arithmetic on top of it.

namespace compositor::render {
namespace {

[[nodiscard]] std::vector<float> box(const std::vector<float>& source, int width, int height, int radius) {
    const float span = static_cast<float>(radius * 2 + 1);
    std::vector<float> pass(static_cast<std::size_t>(width) * height, 0.0F);
    for (int y = 0; y < height; ++y) {
        const std::size_t row = static_cast<std::size_t>(y) * width;
        float sum = 0.0F;
        for (int x = -radius; x <= radius; ++x) {
            sum += source[row + static_cast<std::size_t>(std::clamp(x, 0, width - 1))];
        }
        for (int x = 0; x < width; ++x) {
            pass[row + static_cast<std::size_t>(x)] = sum / span;
            sum -= source[row + static_cast<std::size_t>(std::clamp(x - radius, 0, width - 1))];
            sum += source[row + static_cast<std::size_t>(std::clamp(x + radius + 1, 0, width - 1))];
        }
    }
    std::vector<float> result(static_cast<std::size_t>(width) * height, 0.0F);
    for (int x = 0; x < width; ++x) {
        float sum = 0.0F;
        for (int y = -radius; y <= radius; ++y) {
            sum += pass[static_cast<std::size_t>(std::clamp(y, 0, height - 1)) * width + static_cast<std::size_t>(x)];
        }
        for (int y = 0; y < height; ++y) {
            result[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] = sum / span;
            sum -= pass[static_cast<std::size_t>(std::clamp(y - radius, 0, height - 1)) * width +
                        static_cast<std::size_t>(x)];
            sum += pass[static_cast<std::size_t>(std::clamp(y + radius + 1, 0, height - 1)) * width +
                        static_cast<std::size_t>(x)];
        }
    }
    return result;
}

}  // namespace

std::vector<float> guided_filter(std::vector<float> mask, std::vector<float> guide, int width, int height, int radius,
                                 float epsilon) {
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (count == 0 || mask.size() != count || guide.size() != count || width <= 0 || height <= 0) {
        return mask;
    }
    const std::vector<float> mean_guide = box(guide, width, height, radius);
    const std::vector<float> mean_mask = box(mask, width, height, radius);
    std::vector<float> squares(count, 0.0F);
    std::vector<float> products(count, 0.0F);
    for (std::size_t i = 0; i < count; ++i) {
        squares[i] = guide[i] * guide[i];
        products[i] = guide[i] * mask[i];
    }
    const std::vector<float> mean_squares = box(squares, width, height, radius);
    const std::vector<float> mean_products = box(products, width, height, radius);
    std::vector<float> slope(count, 0.0F);
    std::vector<float> offset(count, 0.0F);
    for (std::size_t i = 0; i < count; ++i) {
        const float variance = mean_squares[i] - mean_guide[i] * mean_guide[i];
        const float covariance = mean_products[i] - mean_guide[i] * mean_mask[i];
        slope[i] = covariance / (variance + epsilon);
        offset[i] = mean_mask[i] - slope[i] * mean_guide[i];
    }
    const std::vector<float> mean_slope = box(slope, width, height, radius);
    const std::vector<float> mean_offset = box(offset, width, height, radius);
    std::vector<float> result(count, 0.0F);
    for (std::size_t i = 0; i < count; ++i) {
        result[i] = std::clamp(mean_slope[i] * guide[i] + mean_offset[i], 0.0F, 1.0F);
    }
    return result;
}

}  // namespace compositor::render
