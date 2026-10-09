#pragma once
#include <vector>

#include "compositor/model/manifest.hpp"

namespace compositor::render {

/// One layer to draw, bottom to top, with the opacity it draws at.
struct DrawItem {
    const model::ProjectLayerRecord* layer = nullptr;
    /// The layer's own opacity multiplied by every enclosing folder's, as the macOS app's
    /// `LayerOpacity` computes it. Folders are pass-through, so descendants are drawn as if the
    /// folder were not there, only dimmer.
    double opacity = 1.0;
    /// True for an adjustment layer, which changes the canvas below it rather than adding pixels.
    bool is_adjustment = false;
};

/// The visible pixel layers of a manifest, in draw order (bottom to top). Folders contribute their
/// opacity to their descendants but are not items themselves; hidden layers and everything inside a
/// hidden folder are left out. Adjustment layers, which affect the canvas below them rather than
/// adding pixels, are not items either and are handled separately.
[[nodiscard]] std::vector<DrawItem> resolve_draw_items(const model::ProjectManifest& manifest);

}  // namespace compositor::render
