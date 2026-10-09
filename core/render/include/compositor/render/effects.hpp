#pragma once
#include <nlohmann/json.hpp>

#include "compositor/render/rgba_surface.hpp"

namespace compositor::render {

/// A layer's pixels with its effects around them, on a canvas grown by `inset` pixels on every side.
/// Callers place it with the layer's transform grown by the same proportion (see the compositor).
struct RenderedEffects {
    RgbaSurface image;
    int inset = 0;
};

/// True when `effects` names at least one effect that is enabled.
[[nodiscard]] bool has_visible_effects(const nlohmann::json& effects);

/// Render the effects around `image`. `mask` is the layer's own raster mask in its pixel grid; when
/// given it hides part of the layer first, so a stroke, shadow and the rest follow the shape actually
/// shown, as in Photoshop. Returns an empty image when there is nothing to draw.
[[nodiscard]] RenderedEffects render_layer_effects(const RgbaSurface& image, const RgbaSurface* mask,
                                                   const nlohmann::json& effects);

}  // namespace compositor::render
