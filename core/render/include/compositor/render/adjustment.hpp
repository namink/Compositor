#pragma once
#include <array>
#include <nlohmann/json.hpp>

#include "compositor/render/rgba_surface.hpp"

namespace compositor::render {

/// Apply an adjustment layer's effect to `canvas`, in place.
///
/// `adjustment` is the manifest's raw `adjustment` object. `opacity` (0–1) is the layer's effective
/// opacity: the effect is blended over the untouched canvas by that amount, so a 50% adjustment is
/// half-strength. The adjustment acts on the accumulated canvas below it in the stack, which is what
/// the macOS app's live renderer does (folders are pass-through, so they do not bound it).
///
/// `mask_coverage`, when given, is a document-sized coverage map (white reveals) that clips the
/// effect: where it is black the canvas is untouched, and soft values blend partway. This mirrors
/// the macOS app's `adjustmentClip`.
void apply_adjustment(const nlohmann::json& adjustment, RgbaSurface& canvas, double opacity,
                      const RgbaSurface* mask_coverage = nullptr);

/// Automatic Levels, from the layer's own pixels: the `levels` settings that stretch each channel's
/// endpoints at 0.1% clipping. `mode` is 0 Contrast (a shared interval), 1 Color (per channel) or
/// 2 Color + neutral midtones (per channel with a gamma). The result is the `levels` object of a
/// Levels adjustment, ready to store or apply.
[[nodiscard]] nlohmann::json auto_levels_settings(const RgbaSurface& layer, int mode);

/// A 4×256 histogram of un-premultiplied color: the RGB composite first, then red, green and blue —
/// what the Levels panel draws (the macOS app's `levels_histogram`). Transparent pixels are skipped.
[[nodiscard]] std::array<double, 1024> histogram(const RgbaSurface& image);

}  // namespace compositor::render
