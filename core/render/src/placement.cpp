#include "compositor/render/placement.hpp"

#include <algorithm>
#include <cmath>

namespace compositor::render {
namespace {

/// A 2x3 affine map from source pixel coordinates to document pixels.
struct Affine {
    double m00 = 1.0;
    double m01 = 0.0;
    double m02 = 0.0;
    double m10 = 0.0;
    double m11 = 1.0;
    double m12 = 0.0;

    [[nodiscard]] bool invertible() const { return std::abs(m00 * m11 - m01 * m10) > 1e-12; }

    /// Source pixel for a document point, or leaves the caller to check the determinant first.
    void invert(double x, double y, double& sx, double& sy) const {
        const double det = m00 * m11 - m01 * m10;
        const double dx = x - m02;
        const double dy = y - m12;
        sx = (m11 * dx - m01 * dy) / det;
        sy = (-m10 * dx + m00 * dy) / det;
    }
};

/// The map that sends a `width` x `height` image into document space through `transform`, mirroring
/// the macOS app's layer placement: the layer's rectangle is centered on the transform's center,
/// scaled from the image's own pixel size to `transform` width/height, flipped, then rotated.
[[nodiscard]] Affine map_for(const model::LayerTransform& transform, int width, int height) {
    const double angle = transform.rotation * 3.14159265358979323846 / 180.0;
    const double cos_a = std::cos(angle);
    const double sin_a = std::sin(angle);
    const double flip_x = transform.flip_x ? -1.0 : 1.0;
    const double flip_y = transform.flip_y ? -1.0 : 1.0;
    const double sx = transform.width * flip_x / static_cast<double>(width);
    const double sy = transform.height * flip_y / static_cast<double>(height);
    const double tx = -0.5 * transform.width * flip_x;
    const double ty = -0.5 * transform.height * flip_y;
    const double cx = transform.center_x();
    const double cy = transform.center_y();

    Affine map;
    map.m00 = cos_a * sx;
    map.m01 = -sin_a * sy;
    map.m02 = cos_a * tx - sin_a * ty + cx;
    map.m10 = sin_a * sx;
    map.m11 = cos_a * sy;
    map.m12 = sin_a * tx + cos_a * ty + cy;
    return map;
}

[[nodiscard]] std::uint8_t channel_at(const RgbaSurface& surface, int x, int y, int channel) {
    const int cx = std::clamp(x, 0, surface.width() - 1);
    const int cy = std::clamp(y, 0, surface.height() - 1);
    return surface.data()[surface.offset(cx, cy) + static_cast<std::size_t>(channel)];
}

/// One channel of `surface` at source pixel (sx, sy), bilinear when `bilinear`.
[[nodiscard]] float sample(const RgbaSurface& surface, double sx, double sy, int channel, bool bilinear) {
    if (!bilinear) {
        return static_cast<float>(
                   channel_at(surface, static_cast<int>(std::floor(sx)), static_cast<int>(std::floor(sy)), channel)) /
               255.0F;
    }
    const double fx = sx - 0.5;
    const double fy = sy - 0.5;
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const float ax = static_cast<float>(fx - x0);
    const float ay = static_cast<float>(fy - y0);
    const float c00 = static_cast<float>(channel_at(surface, x0, y0, channel));
    const float c10 = static_cast<float>(channel_at(surface, x0 + 1, y0, channel));
    const float c01 = static_cast<float>(channel_at(surface, x0, y0 + 1, channel));
    const float c11 = static_cast<float>(channel_at(surface, x0 + 1, y0 + 1, channel));
    const float top = c00 + (c10 - c00) * ax;
    const float bottom = c01 + (c11 - c01) * ax;
    return (top + (bottom - top) * ay) / 255.0F;
}

}  // namespace

RgbaSurface place_layer(const RgbaSurface& image, const model::LayerTransform& transform, bool nearest,
                        const RgbaSurface* mask, const model::LayerTransform* mask_transform, int document_width,
                        int document_height) {
    RgbaSurface result(document_width, document_height);
    if (image.empty() || document_width <= 0 || document_height <= 0) {
        return result;
    }
    const Affine to_image = map_for(transform, image.width(), image.height());
    if (!to_image.invertible()) {
        return result;
    }
    const bool bilinear = !nearest;
    const bool links_mask = mask_transform == nullptr;
    const Affine to_mask = links_mask ? Affine{}
                                      : map_for(*mask_transform, mask != nullptr ? mask->width() : 1,
                                                mask != nullptr ? mask->height() : 1);

    for (int y = 0; y < document_height; ++y) {
        for (int x = 0; x < document_width; ++x) {
            double sx = 0.0;
            double sy = 0.0;
            to_image.invert(static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5, sx, sy);
            if (sx < 0.0 || sy < 0.0 || sx >= static_cast<double>(image.width()) ||
                sy >= static_cast<double>(image.height())) {
                continue;
            }
            float coverage = 1.0F;
            if (mask != nullptr && !mask->empty()) {
                double mx = 0.0;
                double my = 0.0;
                if (links_mask) {
                    // The mask shares the layer's rectangle, so the image's own pixels map straight in.
                    mx = sx / static_cast<double>(image.width()) * static_cast<double>(mask->width());
                    my = sy / static_cast<double>(image.height()) * static_cast<double>(mask->height());
                } else {
                    to_mask.invert(static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5, mx, my);
                }
                if (mx >= 0.0 && my >= 0.0 && mx < static_cast<double>(mask->width()) &&
                    my < static_cast<double>(mask->height())) {
                    coverage = sample(*mask, mx, my, 0, bilinear);
                }
            }
            const float alpha = sample(image, sx, sy, 3, bilinear) * coverage;
            if (alpha <= 0.0F) {
                continue;
            }
            // Pixels are premultiplied, so the mask coverage scales the color channels too.
            const auto scaled = [coverage](float value) {
                return static_cast<std::uint8_t>(std::lround(std::clamp(value * coverage, 0.0F, 1.0F) * 255.0F));
            };
            result.set(x, y, scaled(sample(image, sx, sy, 0, bilinear)), scaled(sample(image, sx, sy, 1, bilinear)),
                       scaled(sample(image, sx, sy, 2, bilinear)),
                       static_cast<std::uint8_t>(std::lround(alpha * 255.0F)));
        }
    }
    return result;
}

RgbaSurface place_mask_coverage(const RgbaSurface& mask, const model::LayerTransform& transform,
                                const model::LayerTransform* mask_transform, int document_width, int document_height) {
    RgbaSurface coverage(document_width, document_height);
    if (mask.empty() || document_width <= 0 || document_height <= 0) {
        return coverage;
    }
    const model::LayerTransform& source_transform = mask_transform != nullptr ? *mask_transform : transform;
    const Affine to_mask = map_for(source_transform, mask.width(), mask.height());
    if (!to_mask.invertible()) {
        return coverage;
    }
    for (int y = 0; y < document_height; ++y) {
        for (int x = 0; x < document_width; ++x) {
            double mx = 0.0;
            double my = 0.0;
            to_mask.invert(static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5, mx, my);
            if (mx < 0.0 || my < 0.0 || mx >= static_cast<double>(mask.width()) ||
                my >= static_cast<double>(mask.height())) {
                continue;
            }
            const float value = sample(mask, mx, my, 0, true);
            const std::uint8_t byte = static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
            coverage.set(x, y, byte, byte, byte, 255);
        }
    }
    return coverage;
}

bool layer_pixel_at(const model::LayerTransform& transform, int image_width, int image_height, double document_x,
                    double document_y, double& pixel_x, double& pixel_y) {
    const Affine map = map_for(transform, image_width, image_height);
    if (!map.invertible()) {
        return false;
    }
    map.invert(document_x, document_y, pixel_x, pixel_y);
    return true;
}

void layer_pixel_to_document(const model::LayerTransform& transform, int image_width, int image_height, double pixel_x,
                             double pixel_y, double& document_x, double& document_y) {
    const Affine map = map_for(transform, image_width, image_height);
    document_x = map.m00 * pixel_x + map.m01 * pixel_y + map.m02;
    document_y = map.m10 * pixel_x + map.m11 * pixel_y + map.m12;
}

model::LayerTransform grow_transform(const model::LayerTransform& transform, int image_width, int image_height,
                                     double inset) {
    const double width = image_width;
    const double height = image_height;
    if (width <= inset * 2.0 || height <= inset * 2.0) {
        return transform;
    }
    model::LayerTransform grown = transform;
    grown.width = transform.width * width / (width - inset * 2.0);
    grown.height = transform.height * height / (height - inset * 2.0);
    grown.origin_x = transform.center_x() - grown.width / 2.0;
    grown.origin_y = transform.center_y() - grown.height / 2.0;
    return grown;
}

}  // namespace compositor::render
