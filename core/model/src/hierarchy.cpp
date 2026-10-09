#include "compositor/model/hierarchy.hpp"

#include <cstddef>
#include <optional>
#include <unordered_set>

#include "compositor/model/errors.hpp"
#include "compositor/model/limits.hpp"

namespace compositor::model {
namespace {

[[noreturn]] void invalid() {
    throw ProjectError(ProjectErrorCode::invalid, "The layer hierarchy is not valid.");
}

}  // namespace

const ProjectLayerRecord* LayerLookup::find(const std::string& id) const {
    if (layers == nullptr) {
        return nullptr;
    }
    const auto found = index.find(id);
    return found == index.end() ? nullptr : &(*layers)[found->second];
}

LayerLookup build_lookup(const std::vector<ProjectLayerRecord>& layers) {
    LayerLookup lookup;
    lookup.layers = &layers;
    lookup.index.reserve(layers.size());
    for (std::size_t i = 0; i < layers.size(); ++i) {
        if (!lookup.index.emplace(layers[i].id, i).second) {
            invalid();
        }
        // A folder carries no pixels of its own.
        if (layers[i].is_group_layer() && layers[i].image_file) {
            invalid();
        }
    }
    return lookup;
}

void validate_hierarchy(const std::vector<ProjectLayerRecord>& layers) {
    const LayerLookup lookup = build_lookup(layers);
    constexpr std::size_t kMaxAncestors = static_cast<std::size_t>(DocumentLimits::kMaxAncestorLevels);
    for (const ProjectLayerRecord& layer : layers) {
        std::unordered_set<std::string> seen;
        seen.insert(layer.id);
        std::optional<std::string> parent = layer.parent_id;
        while (parent) {
            if (seen.size() > kMaxAncestors) {
                invalid();
            }
            if (!seen.insert(*parent).second) {
                invalid();
            }
            const ProjectLayerRecord* node = lookup.find(*parent);
            if (node == nullptr || !node->is_group_layer()) {
                invalid();
            }
            parent = node->parent_id;
        }
        if (layer.is_group_layer() && seen.size() > kMaxAncestors) {
            invalid();
        }
    }
}

double effective_opacity(const ProjectLayerRecord& layer, const LayerLookup& lookup) {
    double opacity = layer.effective_opacity();
    std::optional<std::string> parent = layer.parent_id;
    int depth = 0;
    while (parent && depth < DocumentLimits::kMaxAncestorLevels) {
        const ProjectLayerRecord* folder = lookup.find(*parent);
        if (folder == nullptr) {
            break;
        }
        opacity *= folder->effective_opacity();
        parent = folder->parent_id;
        ++depth;
    }
    return opacity;
}

}  // namespace compositor::model
