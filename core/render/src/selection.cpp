#include "compositor/render/selection.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <vector>

extern "C" {
#include "WandPixels.h"
}

#include "compositor/render/placement.hpp"
#include "effects_internal.hpp"

namespace compositor::render {
namespace {

namespace d = compositor::render::detail;

void recompute_bounds(Selection& selection) {
    selection.min_x = selection.width;
    selection.min_y = selection.height;
    selection.max_x = -1;
    selection.max_y = -1;
    for (int y = 0; y < selection.height; ++y) {
        for (int x = 0; x < selection.width; ++x) {
            if (selection.coverage[static_cast<std::size_t>(y) * selection.width + static_cast<std::size_t>(x)] != 0) {
                selection.min_x = std::min(selection.min_x, x);
                selection.min_y = std::min(selection.min_y, y);
                selection.max_x = std::max(selection.max_x, x);
                selection.max_y = std::max(selection.max_y, y);
            }
        }
    }
}

[[nodiscard]] d::Mask to_mask(const Selection& selection) {
    d::Mask mask(selection.coverage.size());
    for (std::size_t i = 0; i < mask.size(); ++i) {
        mask[i] = static_cast<float>(selection.coverage[i]) / 255.0F;
    }
    return mask;
}

[[nodiscard]] Selection from_mask(const d::Mask& mask, int width, int height) {
    Selection selection;
    selection.width = width;
    selection.height = height;
    selection.coverage.resize(mask.size());
    for (std::size_t i = 0; i < mask.size(); ++i) {
        selection.coverage[i] = static_cast<std::uint8_t>(std::lround(std::clamp(mask[i], 0.0F, 1.0F) * 255.0F));
    }
    recompute_bounds(selection);
    return selection;
}

}  // namespace

Selection from_rect(const DocRect& rect, int document_width, int document_height) {
    Selection selection;
    selection.width = std::max(0, document_width);
    selection.height = std::max(0, document_height);
    selection.coverage.assign(static_cast<std::size_t>(selection.width) * selection.height, 0U);
    if (rect.empty() || selection.width == 0 || selection.height == 0) {
        return selection;
    }
    const int x0 = std::clamp(static_cast<int>(std::floor(rect.x)), 0, selection.width);
    const int y0 = std::clamp(static_cast<int>(std::floor(rect.y)), 0, selection.height);
    const int x1 = std::clamp(static_cast<int>(std::ceil(rect.x + rect.width)), 0, selection.width);
    const int y1 = std::clamp(static_cast<int>(std::ceil(rect.y + rect.height)), 0, selection.height);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            selection.coverage[static_cast<std::size_t>(y) * selection.width + static_cast<std::size_t>(x)] = 255U;
        }
    }
    recompute_bounds(selection);
    return selection;
}

Selection from_ellipse(const DocRect& bounds, int document_width, int document_height) {
    Selection selection;
    selection.width = std::max(0, document_width);
    selection.height = std::max(0, document_height);
    selection.coverage.assign(static_cast<std::size_t>(selection.width) * selection.height, 0U);
    if (bounds.empty() || selection.width == 0 || selection.height == 0) {
        return selection;
    }
    const double cx = bounds.x + bounds.width / 2.0;
    const double cy = bounds.y + bounds.height / 2.0;
    const double rx = bounds.width / 2.0;
    const double ry = bounds.height / 2.0;
    const int x0 = std::clamp(static_cast<int>(std::floor(bounds.x)), 0, selection.width);
    const int y0 = std::clamp(static_cast<int>(std::floor(bounds.y)), 0, selection.height);
    const int x1 = std::clamp(static_cast<int>(std::ceil(bounds.x + bounds.width)), 0, selection.width);
    const int y1 = std::clamp(static_cast<int>(std::ceil(bounds.y + bounds.height)), 0, selection.height);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const double nx = (static_cast<double>(x) + 0.5 - cx) / rx;
            const double ny = (static_cast<double>(y) + 0.5 - cy) / ry;
            if (nx * nx + ny * ny <= 1.0) {
                selection.coverage[static_cast<std::size_t>(y) * selection.width + static_cast<std::size_t>(x)] = 255U;
            }
        }
    }
    recompute_bounds(selection);
    return selection;
}

Selection from_polygon(std::span<const Point> points, int document_width, int document_height) {
    Selection selection;
    selection.width = std::max(0, document_width);
    selection.height = std::max(0, document_height);
    selection.coverage.assign(static_cast<std::size_t>(selection.width) * selection.height, 0U);
    if (points.size() < 3 || selection.width == 0 || selection.height == 0) {
        return selection;
    }
    for (int y = 0; y < selection.height; ++y) {
        const double center = static_cast<double>(y) + 0.5;
        std::vector<double> crossings;
        for (std::size_t i = 0; i < points.size(); ++i) {
            const Point& a = points[i];
            const Point& b = points[(i + 1) % points.size()];
            if ((a.y <= center && b.y > center) || (b.y <= center && a.y > center)) {
                const double t = (center - a.y) / (b.y - a.y);
                crossings.push_back(a.x + t * (b.x - a.x));
            }
        }
        std::sort(crossings.begin(), crossings.end());
        for (std::size_t i = 0; i + 1 < crossings.size(); i += 2) {
            const int xa = std::max(0, static_cast<int>(std::ceil(crossings[i] - 0.5)));
            const int xb = std::min(selection.width - 1, static_cast<int>(std::ceil(crossings[i + 1] - 0.5)) - 1);
            for (int x = xa; x <= xb; ++x) {
                selection.coverage[static_cast<std::size_t>(y) * selection.width + static_cast<std::size_t>(x)] = 255U;
            }
        }
    }
    recompute_bounds(selection);
    return selection;
}

Selection magic_wand(const RgbaSurface& sample, const model::LayerTransform& transform, int document_width,
                     int document_height, double click_x, double click_y, int tolerance, bool contiguous) {
    Selection selection;
    selection.width = std::max(0, document_width);
    selection.height = std::max(0, document_height);
    selection.coverage.assign(static_cast<std::size_t>(selection.width) * selection.height, 0U);
    if (sample.empty() || selection.width == 0 || selection.height == 0) {
        return selection;
    }
    double sample_x = 0.0;
    double sample_y = 0.0;
    if (!layer_pixel_at(transform, sample.width(), sample.height(), click_x, click_y, sample_x, sample_y) ||
        sample_x < 0.0 || sample_y < 0.0 || sample_x >= sample.width() || sample_y >= sample.height()) {
        return selection;
    }
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(sample.width()) * sample.height(), 0U);
    const long count =
        wand_mask(sample.data(), static_cast<std::size_t>(sample.width()), static_cast<std::size_t>(sample.height()),
                  static_cast<std::size_t>(sample.width()) * 4U, static_cast<std::size_t>(std::floor(sample_x)),
                  static_cast<std::size_t>(std::floor(sample_y)), 0, tolerance, contiguous ? 1 : 0, mask.data());
    if (count <= 0) {
        return selection;
    }
    for (int y = 0; y < selection.height; ++y) {
        for (int x = 0; x < selection.width; ++x) {
            double sx = 0.0;
            double sy = 0.0;
            if (!layer_pixel_at(transform, sample.width(), sample.height(), static_cast<double>(x) + 0.5,
                                static_cast<double>(y) + 0.5, sx, sy)) {
                continue;
            }
            if (sx < 0.0 || sy < 0.0 || sx >= sample.width() || sy >= sample.height()) {
                continue;
            }
            const std::size_t index = static_cast<std::size_t>(sy) * sample.width() + static_cast<std::size_t>(sx);
            if (mask[index] != 0) {
                selection.coverage[static_cast<std::size_t>(y) * selection.width + static_cast<std::size_t>(x)] = 255U;
            }
        }
    }
    recompute_bounds(selection);
    return selection;
}

Selection color_range(const RgbaSurface& image, const std::vector<std::uint8_t>& include,
                      const std::vector<std::uint8_t>& exclude, int fuzziness, bool invert) {
    Selection selection;
    selection.width = image.width();
    selection.height = image.height();
    selection.coverage.assign(static_cast<std::size_t>(selection.width) * selection.height, 0U);
    if (image.empty()) {
        return selection;
    }
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(image.width()) * image.height(), 0U);
    const int include_count = static_cast<int>(include.size() / 3);
    const int exclude_count = static_cast<int>(exclude.size() / 3);
    color_range_mask(image.data(), static_cast<std::size_t>(image.width()), static_cast<std::size_t>(image.height()),
                     static_cast<std::size_t>(image.width()) * 4U, include.empty() ? nullptr : include.data(),
                     include_count, exclude.empty() ? nullptr : exclude.data(), exclude_count,
                     std::clamp(fuzziness, 0, 200), invert ? 1 : 0, mask.data());
    selection.coverage = std::move(mask);
    recompute_bounds(selection);
    return selection;
}

Selection combine(const Selection& base, const Selection& other, CombineMode mode) {
    if (mode == CombineMode::replace) {
        return other;
    }
    Selection result = base;
    const int width = std::min(base.width, other.width);
    const int height = std::min(base.height, other.height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t at = static_cast<std::size_t>(y) * result.width + static_cast<std::size_t>(x);
            const int a = base.at(x, y);
            const int b = other.at(x, y);
            int value = 0;
            switch (mode) {
            case CombineMode::add:
                value = std::max(a, b);
                break;
            case CombineMode::subtract:
                value = std::max(0, a - b);
                break;
            case CombineMode::intersect:
                value = std::min(a, b);
                break;
            default:
                value = a;
                break;
            }
            result.coverage[at] = static_cast<std::uint8_t>(value);
        }
    }
    recompute_bounds(result);
    return result;
}

Selection invert(const Selection& selection) {
    Selection result = selection;
    if (result.coverage.empty()) {
        return result;
    }
    for (std::uint8_t& value : result.coverage) {
        value = static_cast<std::uint8_t>(255 - value);
    }
    recompute_bounds(result);
    return result;
}

Selection feather(const Selection& selection, double radius) {
    if (selection.coverage.empty() || radius <= 0.0) {
        return selection;
    }
    return from_mask(d::blur_mask(to_mask(selection), selection.width, selection.height, radius), selection.width,
                     selection.height);
}

Selection expand(const Selection& selection, int radius) {
    if (selection.coverage.empty() || radius <= 0) {
        return selection;
    }
    return from_mask(d::extremes(to_mask(selection), selection.width, selection.height, radius, false), selection.width,
                     selection.height);
}

Selection contract(const Selection& selection, int radius) {
    if (selection.coverage.empty() || radius <= 0) {
        return selection;
    }
    return from_mask(d::extremes(to_mask(selection), selection.width, selection.height, radius, true), selection.width,
                     selection.height);
}

std::vector<std::vector<Point>> outline(const Selection& selection) {
    std::vector<std::vector<Point>> loops;
    if (selection.empty()) {
        return loops;
    }
    std::int32_t* points = nullptr;
    std::size_t point_count = 0;
    std::int32_t* loop_counts = nullptr;
    std::size_t loop_count = 0;
    const int status =
        wand_trace(selection.coverage.data(), static_cast<std::size_t>(selection.width),
                   static_cast<std::size_t>(selection.height), &points, &point_count, &loop_counts, &loop_count);
    if (status == 0 && points != nullptr && loop_counts != nullptr) {
        std::size_t offset = 0;
        for (std::size_t loop = 0; loop < loop_count; ++loop) {
            std::vector<Point> path;
            const std::size_t corners = static_cast<std::size_t>(loop_counts[loop]);
            path.reserve(corners);
            for (std::size_t c = 0; c < corners && offset + 1 < point_count * 2; ++c, offset += 2) {
                path.push_back(Point{static_cast<double>(points[offset]), static_cast<double>(points[offset + 1])});
            }
            if (path.size() >= 3) {
                loops.push_back(std::move(path));
            }
        }
    }
    std::free(points);
    std::free(loop_counts);
    return loops;
}

void clear_in_layer(RgbaSurface& layer, const model::LayerTransform& transform, const Selection& selection) {
    if (layer.empty() || selection.empty()) {
        return;
    }
    std::uint8_t* pixels = layer.data();
    for (int y = 0; y < layer.height(); ++y) {
        for (int x = 0; x < layer.width(); ++x) {
            double doc_x = 0.0;
            double doc_y = 0.0;
            layer_pixel_to_document(transform, layer.width(), layer.height(), static_cast<double>(x) + 0.5,
                                    static_cast<double>(y) + 0.5, doc_x, doc_y);
            const float coverage = static_cast<float>(selection.at(static_cast<int>(std::floor(doc_x)),
                                                                   static_cast<int>(std::floor(doc_y)))) /
                                   255.0F;
            if (coverage <= 0.0F) {
                continue;
            }
            const std::size_t at = layer.offset(x, y);
            const float keep = 1.0F - coverage;
            for (int c = 0; c < 4; ++c) {
                pixels[at + static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(
                    std::lround(static_cast<float>(pixels[at + static_cast<std::size_t>(c)]) * keep));
            }
        }
    }
}

void fill_in_layer(RgbaSurface& layer, const model::LayerTransform& transform, const Selection& selection, double red,
                   double green, double blue, double opacity) {
    if (layer.empty() || selection.empty() || opacity <= 0.0) {
        return;
    }
    std::uint8_t* pixels = layer.data();
    for (int y = 0; y < layer.height(); ++y) {
        for (int x = 0; x < layer.width(); ++x) {
            double doc_x = 0.0;
            double doc_y = 0.0;
            layer_pixel_to_document(transform, layer.width(), layer.height(), static_cast<double>(x) + 0.5,
                                    static_cast<double>(y) + 0.5, doc_x, doc_y);
            const float coverage = static_cast<float>(selection.at(static_cast<int>(std::floor(doc_x)),
                                                                   static_cast<int>(std::floor(doc_y)))) /
                                   255.0F;
            const float alpha = static_cast<float>(opacity) * coverage;
            if (alpha <= 0.0F) {
                continue;
            }
            const std::size_t at = layer.offset(x, y);
            const float keep = 1.0F - alpha;
            const float src[3] = {static_cast<float>(red) * alpha, static_cast<float>(green) * alpha,
                                  static_cast<float>(blue) * alpha};
            for (int c = 0; c < 3; ++c) {
                const float value =
                    src[c] + static_cast<float>(pixels[at + static_cast<std::size_t>(c)]) / 255.0F * keep;
                pixels[at + static_cast<std::size_t>(c)] =
                    static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
            }
            const float out_alpha = alpha + static_cast<float>(pixels[at + 3]) / 255.0F * keep;
            pixels[at + 3] = static_cast<std::uint8_t>(std::lround(std::clamp(out_alpha, 0.0F, 1.0F) * 255.0F));
        }
    }
}

}  // namespace compositor::render
