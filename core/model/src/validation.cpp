#include "compositor/model/validation.hpp"

#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_set>

#include "compositor/model/errors.hpp"
#include "compositor/model/hierarchy.hpp"
#include "compositor/model/limits.hpp"
#include "compositor/model/live_mask.hpp"
#include "compositor/model/uuid.hpp"
#include "validation_internal.hpp"

namespace compositor::model {
namespace {

[[noreturn]] void invalid() {
    throw ProjectError(ProjectErrorCode::invalid, "The project manifest is not valid.");
}

void validate_identity(const ProjectManifest& manifest) {
    if (manifest.format != kFormatIdentifier) {
        invalid();
    }
    if (manifest.version < kMinSupportedFormatVersion || manifest.version > kCurrentFormatVersion) {
        throw ProjectError(ProjectErrorCode::unsupported_version,
                           project_error_message(ProjectErrorCode::unsupported_version, manifest.version),
                           manifest.version);
    }
    if (manifest.color_space != kColorSpace) {
        invalid();
    }
    if (manifest.resolution &&
        (!std::isfinite(*manifest.resolution) || *manifest.resolution < 1.0 || *manifest.resolution > 9600.0)) {
        invalid();
    }
}

void validate_bounds(const ProjectManifest& manifest) {
    const bool sides_ok = manifest.width >= 1 && manifest.width <= DocumentLimits::kMaxSide && manifest.height >= 1 &&
                          manifest.height <= DocumentLimits::kMaxSide;
    if (!sides_ok || manifest.layers.size() > DocumentLimits::kMaxLayers) {
        throw ProjectError(ProjectErrorCode::too_large, project_error_message(ProjectErrorCode::too_large, 0));
    }
}

bool has_any_whitespace_only(const std::string& name) {
    for (const char c : name) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

void validate_layer_identities(const ProjectManifest& manifest) {
    std::unordered_set<std::string> ids;
    ids.reserve(manifest.layers.size());
    for (const ProjectLayerRecord& layer : manifest.layers) {
        const bool unique = ids.insert(layer.id).second;
        const bool name_ok = !layer.name.empty() && !has_any_whitespace_only(layer.name) &&
                             layer.name.size() <= DocumentLimits::kMaxNameBytes;
        const bool image_named = !layer.image_file || *layer.image_file == layer.id + ".png";
        if (!unique || !layer.transform.is_valid() || !name_ok || !image_named) {
            invalid();
        }
        if (manifest.version < 5 && layer.mask_source_id) {
            invalid();
        }
        if (manifest.version == 1 && (layer.parent_id || layer.is_group_layer())) {
            invalid();
        }
    }
    if (manifest.active_layer_id && ids.find(*manifest.active_layer_id) == ids.end()) {
        invalid();
    }
}

void validate_guides(const ProjectManifest& manifest) {
    const std::vector<CanvasGuide> empty;
    const std::vector<CanvasGuide>& guides = manifest.guides ? *manifest.guides : empty;
    if (manifest.version < 8) {
        if (!guides.empty()) {
            invalid();
        }
        return;
    }
    if (guides.size() > static_cast<std::size_t>(DocumentLimits::kMaxGuides)) {
        throw ProjectError(ProjectErrorCode::too_large, project_error_message(ProjectErrorCode::too_large, 0));
    }
    std::unordered_set<std::string> ids;
    for (const CanvasGuide& guide : guides) {
        const bool unique = ids.insert(guide.id).second;
        if (!unique || !std::isfinite(guide.position) || std::abs(guide.position) > DocumentLimits::kMaxGuidePosition) {
            invalid();
        }
    }
}

}  // namespace

void validate_manifest(const ProjectManifest& manifest) {
    validate_identity(manifest);
    validate_bounds(manifest);
    for (const ProjectLayerRecord& layer : manifest.layers) {
        validate_layer_rules(manifest, layer);
    }
    validate_hierarchy(manifest.layers);
    validate_live_masks(manifest.layers);
    validate_layer_identities(manifest);
    validate_guides(manifest);
}

}  // namespace compositor::model
