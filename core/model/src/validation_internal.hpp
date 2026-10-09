#pragma once
#include "compositor/model/layer_record.hpp"
#include "compositor/model/manifest.hpp"

namespace compositor::model {

/// The per-layer rules from `ProjectStore.validate`, split out so one file does not carry the whole
/// validator. Throws `ProjectError` on the first violation.
void validate_layer_rules(const ProjectManifest& manifest, const ProjectLayerRecord& layer);

}  // namespace compositor::model
