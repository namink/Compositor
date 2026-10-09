#include "compositor/render/compositor.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "compositor/render/adjustment.hpp"
#include "compositor/render/blend_mode_math.hpp"
#include "compositor/render/effects.hpp"
#include "compositor/render/placement.hpp"
#include "compositor/render/render_order.hpp"

namespace compositor::render {
namespace {

using model::LayerBlendMode;

[[nodiscard]] std::uint8_t to_byte(float value) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

const RgbaSurface* find_surface(const std::map<std::string, RgbaSurface>& table, const std::string& id) {
    const auto found = table.find(id);
    return found == table.end() ? nullptr : &found->second;
}

}  // namespace

void composite_over(RgbaSurface& canvas, const RgbaSurface& source, LayerBlendMode mode, float opacity) {
    if (canvas.width() != source.width() || canvas.height() != source.height() || opacity <= 0.0F) {
        return;
    }
    std::uint8_t* destination = canvas.data();
    const std::uint8_t* pixels = source.data();
    const std::size_t count = static_cast<std::size_t>(canvas.width()) * static_cast<std::size_t>(canvas.height());
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = i * 4U;
        RgbF cs;
        float source_alpha = 0.0F;
        unpremultiply(pixels[at], pixels[at + 1], pixels[at + 2], pixels[at + 3], cs, source_alpha);
        const float as = source_alpha * opacity;
        if (as <= 0.0F) {
            continue;
        }
        RgbF cb;
        float backdrop_alpha = 0.0F;
        unpremultiply(destination[at], destination[at + 1], destination[at + 2], destination[at + 3], cb,
                      backdrop_alpha);

        const RgbF blended = blend_color(mode, cb, cs);
        // W3C source-over with blend, in premultiplied form. The `as * ab` term applies the blend
        // function only where both the source and the backdrop exist, so a soft brush over nothing
        // stays soft instead of taking a hard edge (the Color Burn/Dodge bug the macOS app works
        // around with Core Image).
        const float weight_source = as * (1.0F - backdrop_alpha);
        const float weight_blend = as * backdrop_alpha;
        const float weight_backdrop = (1.0F - as) * backdrop_alpha;
        const float out_r = weight_source * cs.r + weight_blend * blended.r + weight_backdrop * cb.r;
        const float out_g = weight_source * cs.g + weight_blend * blended.g + weight_backdrop * cb.g;
        const float out_b = weight_source * cs.b + weight_blend * blended.b + weight_backdrop * cb.b;
        const float out_a = as + backdrop_alpha * (1.0F - as);

        destination[at] = to_byte(out_r);
        destination[at + 1] = to_byte(out_g);
        destination[at + 2] = to_byte(out_b);
        destination[at + 3] = to_byte(out_a);
    }
}

RgbaSurface composite_document(const model::ProjectManifest& manifest, const std::map<std::string, RgbaSurface>& images,
                               const std::map<std::string, RgbaSurface>& masks) {
    RgbaSurface canvas(manifest.width, manifest.height);
    const std::vector<DrawItem> items = resolve_draw_items(manifest);
    // Clipping masks (`maskSourceID`): a clipped layer takes its coverage from the base below it, so
    // the base's placed alpha is kept while the stack is drawn.
    std::unordered_set<std::string> base_ids;
    std::unordered_map<std::string, RgbaSurface> placed_bases;
    for (const DrawItem& item : items) {
        if (!item.is_adjustment && item.layer->mask_source_id) {
            base_ids.insert(*item.layer->mask_source_id);
        }
    }
    const auto apply_base_coverage = [](RgbaSurface& placed, const RgbaSurface& base) {
        if (placed.width() != base.width() || placed.height() != base.height()) {
            return;
        }
        std::uint8_t* pixels = placed.data();
        const std::uint8_t* base_pixels = base.data();
        const std::size_t count = static_cast<std::size_t>(placed.width()) * placed.height();
        for (std::size_t i = 0; i < count; ++i) {
            const int coverage = base_pixels[i * 4U + 3U];
            if (coverage == 255) {
                continue;
            }
            for (std::size_t c = 0; c < 4U; ++c) {
                pixels[i * 4U + c] =
                    static_cast<std::uint8_t>((static_cast<unsigned>(pixels[i * 4U + c]) * coverage + 127U) / 255U);
            }
        }
    };
    for (const DrawItem& item : items) {
        if (item.is_adjustment) {
            if (item.layer->adjustment) {
                RgbaSurface coverage;
                const RgbaSurface* coverage_ptr = nullptr;
                if (item.layer->mask_file && item.layer->mask_enabled.value_or(true)) {
                    const RgbaSurface* mask = find_surface(masks, item.layer->id);
                    if (mask != nullptr) {
                        const model::LayerTransform* mask_transform = nullptr;
                        if (item.layer->mask_linked == std::optional<bool>(false) && item.layer->mask_placement) {
                            mask_transform = &*item.layer->mask_placement;
                        }
                        coverage = place_mask_coverage(*mask, item.layer->transform, mask_transform, manifest.width,
                                                       manifest.height);
                        coverage_ptr = &coverage;
                    }
                }
                apply_adjustment(*item.layer->adjustment, canvas, item.opacity, coverage_ptr);
            }
            continue;
        }
        const RgbaSurface* image = find_surface(images, item.layer->id);
        if (image == nullptr) {
            continue;
        }
        const RgbaSurface* mask = nullptr;
        const model::LayerTransform* mask_transform = nullptr;
        if (item.layer->mask_file && item.layer->mask_enabled.value_or(true)) {
            mask = find_surface(masks, item.layer->id);
            if (item.layer->mask_linked == std::optional<bool>(false) && item.layer->mask_placement) {
                mask_transform = &*item.layer->mask_placement;
            }
        }
        const bool nearest = item.layer->transform.sampling == model::LayerSampling::nearest;
        RgbaSurface placed;
        if (item.layer->effects && has_visible_effects(*item.layer->effects)) {
            // A linked mask shapes the effects before they are drawn; an unlinked one is left to placement.
            const RgbaSurface* effect_mask = mask_transform == nullptr ? mask : nullptr;
            const RenderedEffects rendered = render_layer_effects(*image, effect_mask, *item.layer->effects);
            if (!rendered.image.empty()) {
                // The grown transform is computed from the padded image, so its inner (original) part
                // lands exactly where the unpadded layer would.
                const model::LayerTransform grown = grow_transform(item.layer->transform, rendered.image.width(),
                                                                   rendered.image.height(), rendered.inset);
                placed = place_layer(rendered.image, grown, nearest, nullptr, nullptr, manifest.width, manifest.height);
            } else {
                placed = place_layer(*image, item.layer->transform, nearest, mask, mask_transform, manifest.width,
                                     manifest.height);
            }
        } else {
            placed = place_layer(*image, item.layer->transform, nearest, mask, mask_transform, manifest.width,
                                 manifest.height);
        }
        if (item.layer->mask_source_id) {
            const auto base = placed_bases.find(*item.layer->mask_source_id);
            if (base != placed_bases.end()) {
                apply_base_coverage(placed, base->second);
            }
        }
        if (base_ids.count(item.layer->id) != 0) {
            placed_bases[item.layer->id] = placed;
        }
        composite_over(canvas, placed, item.layer->effective_blend_mode(), static_cast<float>(item.opacity));
    }
    return canvas;
}

}  // namespace compositor::render
