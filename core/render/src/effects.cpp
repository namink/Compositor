#include "compositor/render/effects.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "adjustment_json.hpp"
#include "effects_internal.hpp"

namespace compositor::render {
namespace {

using detail::Mask;
using nlohmann::json;

[[nodiscard]] bool enabled(const json& effect) {
    return detail::bool_or(effect, "enabled", true);
}

struct Fill {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    double opacity = 1.0;
};

[[nodiscard]] Fill read_fill(const json& effect, Fill fallback) {
    return Fill{detail::number_or(effect, "red", fallback.red), detail::number_or(effect, "green", fallback.green),
                detail::number_or(effect, "blue", fallback.blue),
                detail::number_or(effect, "opacity", fallback.opacity)};
}

/// A shadow's or inner shadow's offset in layer pixels, y downward: it falls away from the light.
void shadow_offset(const json& effect, double& dx, double& dy) {
    const double angle = detail::number_or(effect, "angle", 90.0);
    const double distance = detail::number_or(effect, "distance", 20.0);
    const double radians = angle * 3.14159265358979323846 / 180.0;
    dx = -std::cos(radians) * distance;
    dy = std::sin(radians) * distance;
}

[[nodiscard]] double margin(const json& effects) {
    double result = 0.0;
    const json& stroke = detail::child(effects, "stroke");
    if (!stroke.empty() && enabled(stroke) && !detail::bool_or(stroke, "inside", false)) {
        result = std::max(result, detail::number_or(stroke, "size", 4.0));
    }
    const json& shadow = detail::child(effects, "shadow");
    if (!shadow.empty() && enabled(shadow)) {
        result = std::max(result,
                          detail::number_or(shadow, "distance", 20.0) + detail::number_or(shadow, "blur", 20.0) * 3.0);
    }
    const json& glow = detail::child(effects, "outerGlow");
    if (!glow.empty() && enabled(glow)) {
        result = std::max(result, detail::number_or(glow, "size", 20.0) * 3.0);
    }
    return std::ceil(result) + 2.0;
}

void apply_mask_to_shown(std::vector<float>& shown, int image_width, int image_height, const RgbaSurface& mask) {
    for (int y = 0; y < image_height; ++y) {
        const int my = std::min(mask.height() - 1, y * mask.height() / image_height);
        for (int x = 0; x < image_width; ++x) {
            const int mx = std::min(mask.width() - 1, x * mask.width() / image_width);
            const float coverage = static_cast<float>(mask.data()[mask.offset(mx, my)]) / 255.0F;
            const std::size_t at = (static_cast<std::size_t>(y) * image_width + static_cast<std::size_t>(x)) * 4U;
            shown[at] *= coverage;
            shown[at + 1] *= coverage;
            shown[at + 2] *= coverage;
            shown[at + 3] *= coverage;
        }
    }
}

}  // namespace

bool has_visible_effects(const nlohmann::json& effects) {
    if (!effects.is_object()) {
        return false;
    }
    for (const char* key : {"stroke", "shadow", "colorOverlay", "innerShadow", "outerGlow", "innerGlow"}) {
        const json& effect = detail::child(effects, key);
        if (!effect.empty() && enabled(effect)) {
            return true;
        }
    }
    return false;
}

RenderedEffects render_layer_effects(const RgbaSurface& image, const RgbaSurface* mask, const nlohmann::json& effects) {
    RenderedEffects result;
    if (image.empty() || !has_visible_effects(effects)) {
        return result;
    }
    const int image_width = image.width();
    const int image_height = image.height();
    const int inset = static_cast<int>(margin(effects));
    const int width = image_width + inset * 2;
    const int height = image_height + inset * 2;
    if (width <= 0 || height <= 0) {
        return result;
    }

    std::vector<float> shown = detail::to_float_rgba(
        image.data(), static_cast<std::size_t>(image_width) * static_cast<std::size_t>(image_height));
    if (mask != nullptr && !mask->empty()) {
        apply_mask_to_shown(shown, image_width, image_height, *mask);
    }
    Mask shown_alpha(static_cast<std::size_t>(image_width) * static_cast<std::size_t>(image_height));
    for (std::size_t i = 0; i < shown_alpha.size(); ++i) {
        shown_alpha[i] = shown[i * 4U + 3U];
    }
    // The shown shape, placed in the padded map at an offset in layer pixels.
    const auto placed_shape = [&](double offset_x, double offset_y) {
        Mask map(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0.0F);
        const int ox = inset + static_cast<int>(std::lround(offset_x));
        const int oy = inset + static_cast<int>(std::lround(offset_y));
        for (int y = 0; y < image_height; ++y) {
            const int ty = y + oy;
            if (ty < 0 || ty >= height) {
                continue;
            }
            for (int x = 0; x < image_width; ++x) {
                const int tx = x + ox;
                if (tx < 0 || tx >= width) {
                    continue;
                }
                map[static_cast<std::size_t>(ty) * width + static_cast<std::size_t>(tx)] =
                    shown_alpha[static_cast<std::size_t>(y) * image_width + static_cast<std::size_t>(x)];
            }
        }
        return map;
    };
    const auto stroke_ring = [&](const Mask& shape, int reach, bool inside) {
        const Mask grown = detail::extremes(shape, width, height, reach, inside);
        Mask ring(shape.size(), 0.0F);
        for (std::size_t i = 0; i < shape.size(); ++i) {
            ring[i] = inside ? std::max(0.0F, shape[i] - grown[i]) : std::max(0.0F, grown[i] - shape[i]);
        }
        return ring;
    };

    std::vector<float> canvas(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0.0F);

    const json& shadow = detail::child(effects, "shadow");
    if (!shadow.empty() && enabled(shadow)) {
        const Fill fill = read_fill(shadow, Fill{0, 0, 0, 0.5});
        if (fill.opacity > 0.0) {
            double dx = 0.0;
            double dy = 0.0;
            shadow_offset(shadow, dx, dy);
            const Mask shape = placed_shape(dx, dy);
            const Mask soft = detail::blur_mask(shape, width, height, detail::number_or(shadow, "blur", 20.0) / 2.0);
            detail::fill_over(canvas, width, height, fill.red, fill.green, fill.blue, fill.opacity, soft);
        }
    }
    const json& outer_glow = detail::child(effects, "outerGlow");
    if (!outer_glow.empty() && enabled(outer_glow)) {
        const Fill fill = read_fill(outer_glow, Fill{1, 1, 1, 0.75});
        if (fill.opacity > 0.0) {
            const Mask shape = placed_shape(0, 0);
            const Mask soft =
                detail::blur_mask(shape, width, height, detail::number_or(outer_glow, "size", 20.0) / 2.0);
            Mask levels(shape.size(), 0.0F);
            for (std::size_t i = 0; i < shape.size(); ++i) {
                levels[i] = std::max(0.0F, soft[i] * (1.0F - shape[i]));
            }
            detail::fill_over(canvas, width, height, fill.red, fill.green, fill.blue, fill.opacity, levels);
        }
    }
    const json& stroke = detail::child(effects, "stroke");
    const bool stroke_on = !stroke.empty() && enabled(stroke) && detail::number_or(stroke, "size", 4.0) > 0.0 &&
                           detail::number_or(stroke, "opacity", 1.0) > 0.0;
    const bool stroke_inside = detail::bool_or(stroke, "inside", false);
    if (stroke_on && !stroke_inside) {
        const Fill fill = read_fill(stroke, Fill{0, 0, 0, 1});
        const Mask ring =
            stroke_ring(placed_shape(0, 0),
                        std::max(1, static_cast<int>(std::lround(detail::number_or(stroke, "size", 4.0)))), false);
        detail::fill_over(canvas, width, height, fill.red, fill.green, fill.blue, fill.opacity, ring);
    }

    // The layer's own pixels, over the shadow, glow and outside stroke.
    detail::draw_over(canvas, width, height, shown, image_width, image_height, inset, inset);

    const json& overlay = detail::child(effects, "colorOverlay");
    if (!overlay.empty() && enabled(overlay)) {
        const Fill fill = read_fill(overlay, Fill{0, 0, 0, 1});
        if (fill.opacity > 0.0) {
            detail::fill_over(canvas, width, height, fill.red, fill.green, fill.blue, fill.opacity, placed_shape(0, 0));
        }
    }
    const json& inner_glow = detail::child(effects, "innerGlow");
    if (!inner_glow.empty() && enabled(inner_glow)) {
        const Fill fill = read_fill(inner_glow, Fill{1, 1, 1, 0.75});
        if (fill.opacity > 0.0) {
            const Mask shape = placed_shape(0, 0);
            const Mask soft =
                detail::blur_mask(shape, width, height, detail::number_or(inner_glow, "size", 10.0) / 2.0);
            Mask inside(shape.size(), 0.0F);
            for (std::size_t i = 0; i < shape.size(); ++i) {
                inside[i] = std::max(0.0F, shape[i] * (1.0F - soft[i]));
            }
            detail::fill_over(canvas, width, height, fill.red, fill.green, fill.blue, fill.opacity, inside);
        }
    }
    const json& inner_shadow = detail::child(effects, "innerShadow");
    if (!inner_shadow.empty() && enabled(inner_shadow)) {
        const Fill fill = read_fill(inner_shadow, Fill{0, 0, 0, 0.5});
        if (fill.opacity > 0.0) {
            double dx = 0.0;
            double dy = 0.0;
            shadow_offset(inner_shadow, dx, dy);
            const Mask shape = placed_shape(0, 0);
            const Mask moved = detail::blur_mask(placed_shape(dx, dy), width, height,
                                                 detail::number_or(inner_shadow, "blur", 10.0) / 2.0);
            Mask inside(shape.size(), 0.0F);
            for (std::size_t i = 0; i < shape.size(); ++i) {
                inside[i] = std::max(0.0F, shape[i] * (1.0F - moved[i]));
            }
            detail::fill_over(canvas, width, height, fill.red, fill.green, fill.blue, fill.opacity, inside);
        }
    }
    if (stroke_on && stroke_inside) {
        const Fill fill = read_fill(stroke, Fill{0, 0, 0, 1});
        const Mask ring =
            stroke_ring(placed_shape(0, 0),
                        std::max(1, static_cast<int>(std::lround(detail::number_or(stroke, "size", 4.0)))), true);
        detail::fill_over(canvas, width, height, fill.red, fill.green, fill.blue, fill.opacity, ring);
    }

    result.image = detail::from_float_rgba(canvas, width, height);
    result.inset = inset;
    return result;
}

}  // namespace compositor::render
