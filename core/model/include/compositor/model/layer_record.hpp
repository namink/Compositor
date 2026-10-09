#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "compositor/model/blend_mode.hpp"
#include "compositor/model/layer_transform.hpp"

namespace compositor::model {

/// One layer (or folder) record in the manifest.
///
/// Fields that arrived with later format versions are optional; a missing value means the version's
/// default (visible, full opacity, Normal blend, no mask, and so on). The four nested feature
/// objects (`adjustment`, `effects`, `text`, `shape`) are held verbatim so a load/save round trip
/// never drops a field this build does not model yet; validation checks their presence and the
/// version gates that depend on them.
struct ProjectLayerRecord {
    std::string id;
    std::string name;
    bool is_visible = true;
    LayerTransform transform;
    std::optional<std::string> image_file;

    std::optional<std::string> parent_id;
    std::optional<bool> is_group;
    std::optional<double> opacity;
    std::optional<LayerBlendMode> blend_mode;
    std::optional<std::string> mask_file;
    std::optional<bool> mask_enabled;
    std::optional<std::string> mask_source_id;
    std::optional<LayerTransform> mask_placement;
    std::optional<bool> mask_linked;

    std::optional<nlohmann::json> adjustment;
    std::optional<nlohmann::json> effects;
    std::optional<nlohmann::json> text;
    std::optional<nlohmann::json> shape;

    [[nodiscard]] bool is_group_layer() const { return is_group.value_or(false); }
    [[nodiscard]] double effective_opacity() const { return opacity.value_or(1.0); }
    [[nodiscard]] LayerBlendMode effective_blend_mode() const { return blend_mode.value_or(LayerBlendMode::normal); }
};

}  // namespace compositor::model
