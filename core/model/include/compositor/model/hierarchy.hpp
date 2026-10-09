#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "compositor/model/layer_record.hpp"

namespace compositor::model {

/// An index over a manifest's layers, for the hierarchy and live-mask walkers.
struct LayerLookup {
    const std::vector<ProjectLayerRecord>* layers = nullptr;
    std::unordered_map<std::string, std::size_t> index;

    [[nodiscard]] const ProjectLayerRecord* find(const std::string& id) const;
};

/// Build the id -> record index, rejecting duplicate ids. Throws `ProjectError`.
[[nodiscard]] LayerLookup build_lookup(const std::vector<ProjectLayerRecord>& layers);

/// Validate folder structure and depth. Throws `ProjectError`.
///
/// A folder must not carry image pixels, every parent must exist and be a folder, no cycle may form
/// and no layer may nest deeper than `DocumentLimits::kMaxAncestorLevels`.
void validate_hierarchy(const std::vector<ProjectLayerRecord>& layers);

/// A folder's opacity multiplies into everything inside it: a layer at 50% in a folder at 50% shows
/// at 25%, while the layer itself still reads 50% in the panel. Folders are pass-through, so the
/// folder's opacity is applied to each descendant rather than to the folder as a whole.
[[nodiscard]] double effective_opacity(const ProjectLayerRecord& layer, const LayerLookup& lookup);

}  // namespace compositor::model
