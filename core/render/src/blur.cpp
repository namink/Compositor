#include "compositor/render/blur.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace compositor::render {
namespace {

constexpr double kPi = 3.14159265358979323846;

[[nodiscard]] std::vector<float> to_float(const RgbaSurface& surface) {
    std::vector<float> values(static_cast<std::size_t>(surface.width()) * static_cast<std::size_t>(surface.height()) *
                              4U);
    const std::uint8_t* bytes = surface.data();
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<float>(bytes[i]) / 255.0F;
    }
    return values;
}

[[nodiscard]] RgbaSurface to_surface(const std::vector<float>& values, int width, int height) {
    RgbaSurface surface(width, height);
    std::uint8_t* bytes = surface.data();
    for (std::size_t i = 0; i < values.size(); ++i) {
        bytes[i] = static_cast<std::uint8_t>(std::lround(std::clamp(values[i], 0.0F, 1.0F) * 255.0F));
    }
    return surface;
}

/// A normalized Gaussian kernel of radius `ceil(3 * sigma)`.
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

/// Premultiplied sample at a fractional point, transparent outside the surface.
[[nodiscard]] float sample_bilinear_zero(const std::vector<float>& image, int width, int height, double x, double y,
                                         int channel) {
    const double fx = x - 0.5;
    const double fy = y - 0.5;
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const float ax = static_cast<float>(fx - x0);
    const float ay = static_cast<float>(fy - y0);
    const auto at = [&](int px, int py) -> float {
        if (px < 0 || py < 0 || px >= width || py >= height) {
            return 0.0F;
        }
        return image[(static_cast<std::size_t>(py) * static_cast<std::size_t>(width) + static_cast<std::size_t>(px)) *
                         4U +
                     static_cast<std::size_t>(channel)];
    };
    const float top = at(x0, y0) + (at(x0 + 1, y0) - at(x0, y0)) * ax;
    const float bottom = at(x0, y0 + 1) + (at(x0 + 1, y0 + 1) - at(x0, y0 + 1)) * ax;
    return top + (bottom - top) * ay;
}

}  // namespace

RgbaSurface gaussian_blur(const RgbaSurface& source, double sigma) {
    if (source.empty() || sigma <= 0.0) {
        return source;
    }
    const int width = source.width();
    const int height = source.height();
    int radius = 0;
    const std::vector<float> kernel = gaussian_kernel(sigma, radius);
    const std::vector<float> input = to_float(source);
    std::vector<float> horizontal(input.size(), 0.0F);
    std::vector<float> output(input.size(), 0.0F);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < 4; ++c) {
                float total = 0.0F;
                for (int i = -radius; i <= radius; ++i) {
                    const int sx = x + i;
                    if (sx < 0 || sx >= width) {
                        continue;
                    }
                    const std::size_t at = (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(sx)) * 4U +
                                           static_cast<std::size_t>(c);
                    total += input[at] * kernel[static_cast<std::size_t>(i + radius)];
                }
                horizontal[(static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4U +
                           static_cast<std::size_t>(c)] = total;
            }
        }
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < 4; ++c) {
                float total = 0.0F;
                for (int i = -radius; i <= radius; ++i) {
                    const int sy = y + i;
                    if (sy < 0 || sy >= height) {
                        continue;
                    }
                    const std::size_t at = (static_cast<std::size_t>(sy) * width + static_cast<std::size_t>(x)) * 4U +
                                           static_cast<std::size_t>(c);
                    total += horizontal[at] * kernel[static_cast<std::size_t>(i + radius)];
                }
                output[(static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4U +
                       static_cast<std::size_t>(c)] = total;
            }
        }
    }
    return to_surface(output, width, height);
}

RgbaSurface motion_blur(const RgbaSurface& source, double distance, double angle_degrees) {
    if (source.empty() || distance <= 0.0) {
        return source;
    }
    const int width = source.width();
    const int height = source.height();
    const double sigma = distance / std::sqrt(12.0);
    int radius = 0;
    const std::vector<float> kernel = gaussian_kernel(sigma, radius);
    const double radians = angle_degrees * kPi / 180.0;
    const double dx = std::cos(radians);
    const double dy = std::sin(radians);

    const std::vector<float> input = to_float(source);
    std::vector<float> output(input.size(), 0.0F);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < 4; ++c) {
                float total = 0.0F;
                for (int i = -radius; i <= radius; ++i) {
                    const double sx = static_cast<double>(x) + 0.5 + dx * i;
                    const double sy = static_cast<double>(y) + 0.5 + dy * i;
                    total += sample_bilinear_zero(input, width, height, sx, sy, c) *
                             kernel[static_cast<std::size_t>(i + radius)];
                }
                output[(static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4U +
                       static_cast<std::size_t>(c)] = total;
            }
        }
    }
    return to_surface(output, width, height);
}

}  // namespace compositor::render
