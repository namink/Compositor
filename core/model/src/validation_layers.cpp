#include <cmath>
#include <string_view>

#include "compositor/model/errors.hpp"
#include "validation_internal.hpp"

namespace compositor::model {
namespace {

using nlohmann::json;

[[noreturn]] void reject() {
    throw ProjectError(ProjectErrorCode::invalid, "A layer record is not valid.");
}

bool has_value(const json& object, const char* key) {
    return object.contains(key) && !object.at(key).is_null();
}

/// Adjustment kinds that sample neighboring pixels, added in format version 9.
bool is_sampling_adjustment(std::string_view kind) {
    return kind == "Gaussian Blur" || kind == "Motion Blur" || kind == "Add Noise";
}

bool is_known_adjustment_kind(std::string_view kind) {
    return kind == "Hue/Saturation" || kind == "Levels" || kind == "Curves" || kind == "Exposure" ||
           kind == "Gradient Map" || kind == "Grain" || kind == "Invert" || kind == "Black & White" ||
           kind == "Color Balance" || is_sampling_adjustment(kind);
}

/// A lightweight structural check for an adjustment object. The full per-kind range validation
/// (hue ±360, curve point ordering and so on) is ported with the adjustment code itself; here the
/// kind must at least be one this format knows and the object must carry a kind.
bool adjustment_is_well_formed(const json& adjustment, std::string& kind_out) {
    if (!adjustment.is_object() || !has_value(adjustment, "kind") || !adjustment.at("kind").is_string()) {
        return false;
    }
    // Held by value: the JSON accessor returns a temporary, and a view over it would dangle.
    kind_out = adjustment.at("kind").get<std::string>();
    return is_known_adjustment_kind(kind_out);
}

/// A lightweight structural check for a text object. Per-letter color and font runs are the fields
/// the version gates care about; the rest is validated with the text tool.
bool text_is_well_formed(const json& text) {
    return text.is_object();
}

void validate_text(const ProjectManifest& manifest, const ProjectLayerRecord& layer) {
    if (!layer.text) {
        return;
    }
    const json& text = *layer.text;
    const bool runs_versioned = (has_value(text, "colorRuns") ? manifest.version >= 10 : true) &&
                                (has_value(text, "fontRuns") ? manifest.version >= 11 : true);
    const bool placement_ok = layer.image_file.has_value() && !layer.is_group_layer() && !layer.adjustment.has_value();
    if (!text_is_well_formed(text) || !runs_versioned || !placement_ok) {
        reject();
    }
}

void validate_adjustment(const ProjectManifest& manifest, const ProjectLayerRecord& layer) {
    if (!layer.adjustment) {
        return;
    }
    if (manifest.version < 7 || layer.is_group_layer() || layer.image_file.has_value()) {
        reject();
    }
    std::string kind;
    if (!adjustment_is_well_formed(*layer.adjustment, kind)) {
        reject();
    }
    if (is_sampling_adjustment(kind) && manifest.version < 9) {
        reject();
    }
}

void validate_mask(const ProjectManifest& manifest, const ProjectLayerRecord& layer) {
    if (layer.mask_file) {
        const int earliest = layer.is_group_layer() ? 6 : 4;
        const bool versioned = manifest.version >= earliest;
        const bool named = *layer.mask_file == layer.id + ".mask.png";
        if (!versioned || !named) {
            reject();
        }
    } else if (layer.mask_enabled.has_value()) {
        // maskEnabled without a maskFile is meaningless.
        reject();
    }
    if (layer.mask_placement) {
        if (!layer.mask_file.has_value() || !layer.mask_placement->is_valid()) {
            reject();
        }
    }
}

void validate_appearance(const ProjectManifest& manifest, const ProjectLayerRecord& layer) {
    const double opacity = layer.effective_opacity();
    const LayerBlendMode blend = layer.effective_blend_mode();
    const bool in_range = std::isfinite(opacity) && opacity >= 0.0 && opacity <= 1.0;
    const bool versioned = manifest.version >= 3 || (opacity == 1.0 && blend == LayerBlendMode::normal);
    const bool folder_ok =
        !layer.is_group_layer() || (blend == LayerBlendMode::normal && (manifest.version >= 8 || opacity == 1.0));
    if (!in_range || !versioned || !folder_ok) {
        reject();
    }
}

}  // namespace

void validate_layer_rules(const ProjectManifest& manifest, const ProjectLayerRecord& layer) {
    validate_text(manifest, layer);
    validate_adjustment(manifest, layer);
    validate_mask(manifest, layer);
    validate_appearance(manifest, layer);
}

}  // namespace compositor::model
