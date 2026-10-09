#pragma once
#include "compositor/model/manifest.hpp"

namespace compositor::model {

/// Enforce every rule a manifest must satisfy before it can be loaded. Throws `ProjectError` on the
/// first violation. Mirrors `ProjectStore.validate` in the macOS app: an invalid document is
/// rejected whole, never partially applied.
void validate_manifest(const ProjectManifest& manifest);

}  // namespace compositor::model
