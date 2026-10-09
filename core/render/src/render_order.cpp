#include "compositor/render/render_order.hpp"

#include <string>
#include <unordered_map>

namespace compositor::render {
namespace {

using model::ProjectLayerRecord;

constexpr const char* kRoot = "";

[[nodiscard]] std::string parent_key(const ProjectLayerRecord& layer) {
    return layer.parent_id ? *layer.parent_id : std::string(kRoot);
}

void visit(const std::string& parent, bool shown, double inherited_opacity,
           const std::unordered_map<std::string, std::vector<const ProjectLayerRecord*>>& children,
           std::vector<DrawItem>& out) {
    const auto found = children.find(parent);
    if (found == children.end()) {
        return;
    }
    for (const ProjectLayerRecord* layer : found->second) {
        const bool visible = shown && layer->is_visible;
        const double opacity = inherited_opacity * layer->effective_opacity();
        if (layer->is_group_layer()) {
            // A hidden folder hides its whole subtree, so there is nothing to walk.
            if (visible) {
                visit(layer->id, visible, opacity, children, out);
            }
        } else if (visible && layer->adjustment) {
            out.push_back(DrawItem{layer, opacity, true});
        } else if (visible && layer->image_file) {
            out.push_back(DrawItem{layer, opacity, false});
        }
    }
}

}  // namespace

std::vector<DrawItem> resolve_draw_items(const model::ProjectManifest& manifest) {
    std::unordered_map<std::string, std::vector<const ProjectLayerRecord*>> children;
    for (const ProjectLayerRecord& layer : manifest.layers) {
        children[parent_key(layer)].push_back(&layer);
    }
    std::vector<DrawItem> items;
    items.reserve(manifest.layers.size());
    visit(kRoot, true, 1.0, children, items);
    return items;
}

}  // namespace compositor::render
