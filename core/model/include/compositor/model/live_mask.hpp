#pragma once
#include <vector>

#include "compositor/model/layer_record.hpp"

namespace compositor::model {

/// Clipping masks, stored as `maskSourceID`: the id of a non-folder layer whose alpha supplies the
/// target's coverage in document space.
///
/// Every referenced source must exist, be a non-folder layer with no adjustment, and not form a
/// cycle; no chain may run longer than `DocumentLimits::kMaxLiveMaskChain`. Throws `ProjectError`.
void validate_live_masks(const std::vector<ProjectLayerRecord>& layers);

}  // namespace compositor::model
