#pragma once
#include <map>
#include <string>

#include "compositor/model/manifest.hpp"
#include "compositor/render/rgba_surface.hpp"

namespace compositor::render {

/// Flatten a document to a single surface, the way export and the base of the canvas do.
///
/// `images` and `masks` hold each layer's decoded pixels keyed by layer id. Layers are drawn bottom
/// to top; folders are pass-through (their opacity multiplies into their descendants); hidden layers
/// and hidden folders are skipped. Adjustment layers and layer effects are not applied yet — they
/// arrive with the adjustment and effect modules.
[[nodiscard]] RgbaSurface composite_document(const model::ProjectManifest& manifest,
                                             const std::map<std::string, RgbaSurface>& images,
                                             const std::map<std::string, RgbaSurface>& masks);

/// Composite `source` over `canvas`, in place, using the W3C source-over-with-blend formula in sRGB.
///
/// `opacity` scales the source's coverage. Both surfaces are premultiplied and the same size.
/// Exposed for tests and for the effect and adjustment renderers that will build on it.
void composite_over(RgbaSurface& canvas, const RgbaSurface& source, model::LayerBlendMode mode, float opacity);

}  // namespace compositor::render
