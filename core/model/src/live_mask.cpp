#include "compositor/model/live_mask.hpp"

#include <cstddef>
#include <optional>
#include <unordered_set>

#include "compositor/model/errors.hpp"
#include "compositor/model/hierarchy.hpp"
#include "compositor/model/limits.hpp"

namespace compositor::model {
namespace {

[[noreturn]] void invalid() {
    throw ProjectError(ProjectErrorCode::invalid, "A clipping-mask link is not valid.");
}

}  // namespace

void validate_live_masks(const std::vector<ProjectLayerRecord>& layers) {
    const LayerLookup lookup = build_lookup(layers);
    constexpr std::size_t kMaxChain = static_cast<std::size_t>(DocumentLimits::kMaxLiveMaskChain);
    for (const ProjectLayerRecord& layer : layers) {
        std::unordered_set<std::string> path;
        std::optional<std::string> current = layer.id;
        while (current) {
            if (path.size() >= kMaxChain) {
                invalid();
            }
            if (!path.insert(*current).second) {
                invalid();
            }
            const ProjectLayerRecord* record = lookup.find(*current);
            if (record == nullptr) {
                invalid();
            }
            if (record->mask_source_id) {
                // A folder cannot be clipped, nor supply a live mask.
                if (record->is_group_layer()) {
                    invalid();
                }
                const ProjectLayerRecord* source = lookup.find(*record->mask_source_id);
                if (source == nullptr || source->is_group_layer() || source->adjustment.has_value()) {
                    invalid();
                }
            }
            current = record->mask_source_id;
        }
    }
}

}  // namespace compositor::model
