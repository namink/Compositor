#include <algorithm>
#include <cmath>
#include <cstddef>

#include "effects_internal.hpp"

namespace compositor::render::detail {
namespace {

[[nodiscard]] std::vector<float> gaussian_kernel(double sigma, int& radius) {
    radius = std::max(1, static_cast<int>(std::ceil(sigma * 3.0)));
    std::vector<float> kernel(static_cast<std::size_t>(2 * radius + 1));
    const double denominator = 2.0 * sigma * sigma;
    double total = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        const double value = std::exp(-static_cast<double>(i * i) / denominator);
        kernel[static_cast<std::size_t>(i + radius)] = static_cast<float>(value);
        total += value;
    }
    for (float& value : kernel) {
        value = static_cast<float>(static_cast<double>(value) / total);
    }
    return kernel;
}

}  // namespace

Mask blur_mask(const Mask& mask, int width, int height, double sigma) {
    if (sigma <= 0.0 || width <= 0 || height <= 0) {
        return mask;
    }
    int radius = 0;
    const std::vector<float> kernel = gaussian_kernel(sigma, radius);
    Mask horizontal(mask.size(), 0.0F);
    Mask result(mask.size(), 0.0F);
    const auto at = [&](int x, int y) {
        // Clamp to the edge, as `clampedToExtent` does.
        const int cx = std::clamp(x, 0, width - 1);
        const int cy = std::clamp(y, 0, height - 1);
        return mask[static_cast<std::size_t>(cy) * width + static_cast<std::size_t>(cx)];
    };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float total = 0.0F;
            for (int i = -radius; i <= radius; ++i) {
                total += at(x + i, y) * kernel[static_cast<std::size_t>(i + radius)];
            }
            horizontal[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] = total;
        }
    }
    const auto at_h = [&](int x, int y) {
        const int cx = std::clamp(x, 0, width - 1);
        const int cy = std::clamp(y, 0, height - 1);
        return horizontal[static_cast<std::size_t>(cy) * width + static_cast<std::size_t>(cx)];
    };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float total = 0.0F;
            for (int i = -radius; i <= radius; ++i) {
                total += at_h(x, y + i) * kernel[static_cast<std::size_t>(i + radius)];
            }
            result[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] = total;
        }
    }
    return result;
}

Mask extremes(const Mask& source, int width, int height, int reach, bool smallest) {
    if (width <= 0 || height <= 0 ||
        source.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return {};
    }
    const int radius = std::max(0, reach);
    Mask pass(source.size(), 0.0F);
    Mask result(source.size(), 0.0F);
    std::vector<int> queue(static_cast<std::size_t>(std::max(width, height)));

    // A monotonic deque sweeps each line so every index enters and leaves once, independent of reach.
    const auto sweep = [&](const Mask& input, Mask& output, int lines, int count, int line_step, int element_step) {
        for (int line = 0; line < lines; ++line) {
            const int base = line * line_step;
            int head = 0;
            int tail = 0;
            int next = 0;
            for (int center = 0; center < count; ++center) {
                while (next <= std::min(count - 1, center + radius)) {
                    const float value = input[static_cast<std::size_t>(base + next * element_step)];
                    while (tail > head) {
                        const float previous = input[static_cast<std::size_t>(
                            base + queue[static_cast<std::size_t>(tail - 1)] * element_step)];
                        if (smallest ? previous < value : previous > value) {
                            break;
                        }
                        --tail;
                    }
                    queue[static_cast<std::size_t>(tail)] = next;
                    ++tail;
                    ++next;
                }
                while (head < tail && queue[static_cast<std::size_t>(head)] < center - radius) {
                    ++head;
                }
                const bool outside = center < radius || center + radius >= count;
                output[static_cast<std::size_t>(base + center * element_step)] =
                    smallest && outside
                        ? 0.0F
                        : input[static_cast<std::size_t>(base + queue[static_cast<std::size_t>(head)] * element_step)];
            }
        }
    };
    sweep(source, pass, height, width, width, 1);
    sweep(pass, result, width, height, 1, width);
    return result;
}

void fill_over(std::vector<float>& destination, int width, int height, double red, double green, double blue,
               double opacity, const Mask& coverage) {
    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (std::size_t i = 0; i < pixels; ++i) {
        const float alpha = static_cast<float>(opacity) * coverage[i];
        if (alpha <= 0.0F) {
            continue;
        }
        const std::size_t at = i * 4U;
        const float keep = 1.0F - alpha;
        destination[at] = static_cast<float>(red) * alpha + destination[at] * keep;
        destination[at + 1] = static_cast<float>(green) * alpha + destination[at + 1] * keep;
        destination[at + 2] = static_cast<float>(blue) * alpha + destination[at + 2] * keep;
        destination[at + 3] = alpha + destination[at + 3] * keep;
    }
}

void draw_over(std::vector<float>& destination, int destination_width, int destination_height,
               const std::vector<float>& source, int source_width, int source_height, int dx, int dy) {
    for (int y = 0; y < source_height; ++y) {
        const int ty = y + dy;
        if (ty < 0 || ty >= destination_height) {
            continue;
        }
        for (int x = 0; x < source_width; ++x) {
            const int tx = x + dx;
            if (tx < 0 || tx >= destination_width) {
                continue;
            }
            const std::size_t s = (static_cast<std::size_t>(y) * source_width + static_cast<std::size_t>(x)) * 4U;
            const std::size_t d =
                (static_cast<std::size_t>(ty) * destination_width + static_cast<std::size_t>(tx)) * 4U;
            const float src_alpha = source[s + 3];
            const float keep = 1.0F - src_alpha;
            destination[d] = source[s] + destination[d] * keep;
            destination[d + 1] = source[s + 1] + destination[d + 1] * keep;
            destination[d + 2] = source[s + 2] + destination[d + 2] * keep;
            destination[d + 3] = src_alpha + destination[d + 3] * keep;
        }
    }
}

Mask shape_coverage(const std::uint8_t* premultiplied, int image_width, int image_height, int map_width, int map_height,
                    int offset_x, int offset_y) {
    Mask mask(static_cast<std::size_t>(map_width) * static_cast<std::size_t>(map_height), 0.0F);
    for (int y = 0; y < image_height; ++y) {
        const int ty = y + offset_y;
        if (ty < 0 || ty >= map_height) {
            continue;
        }
        for (int x = 0; x < image_width; ++x) {
            const int tx = x + offset_x;
            if (tx < 0 || tx >= map_width) {
                continue;
            }
            const std::size_t s = (static_cast<std::size_t>(y) * image_width + static_cast<std::size_t>(x)) * 4U;
            mask[static_cast<std::size_t>(ty) * map_width + static_cast<std::size_t>(tx)] =
                static_cast<float>(premultiplied[s + 3]) / 255.0F;
        }
    }
    return mask;
}

std::vector<float> to_float_rgba(const std::uint8_t* bytes, std::size_t pixels) {
    std::vector<float> values(pixels * 4U);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<float>(bytes[i]) / 255.0F;
    }
    return values;
}

RgbaSurface from_float_rgba(const std::vector<float>& values, int width, int height) {
    RgbaSurface surface(width, height);
    std::uint8_t* bytes = surface.data();
    for (std::size_t i = 0; i < values.size(); ++i) {
        bytes[i] = static_cast<std::uint8_t>(std::lround(std::clamp(values[i], 0.0F, 1.0F) * 255.0F));
    }
    return surface;
}

}  // namespace compositor::render::detail
