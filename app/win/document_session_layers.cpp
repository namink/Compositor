#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "compositor/render/compositor.hpp"
#include "document_session.hpp"

// Layer reordering and reparenting from the Layers panel, split out so `document_session_edit.cpp`
// stays within the soft size limit.

namespace compositor::appwin {

bool DocumentSession::reorder_layers(const std::vector<std::string>& top_first_ids, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    std::vector<model::ProjectLayerRecord>& layers = snapshot_->manifest.layers;
    if (top_first_ids.size() != layers.size()) {
        error = QStringLiteral("The layer order did not match the project.");
        return false;
    }
    std::map<std::string, const model::ProjectLayerRecord*> by_id;
    for (const model::ProjectLayerRecord& record : layers) {
        by_id.emplace(record.id, &record);
    }
    // The panel lists layers top-first; the manifest stores them bottom-first.
    std::vector<model::ProjectLayerRecord> reordered;
    reordered.reserve(layers.size());
    for (auto it = top_first_ids.rbegin(); it != top_first_ids.rend(); ++it) {
        const auto found = by_id.find(*it);
        if (found == by_id.end()) {
            error = QStringLiteral("The layer order did not match the project.");
            return false;
        }
        reordered.push_back(*found->second);
    }
    push_history();
    layers = std::move(reordered);
    release_detached_clipping();
    return recomposite(error);
}

bool DocumentSession::set_layer_tree(const std::vector<std::pair<std::string, std::optional<std::string>>>& top_first,
                                     QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    std::vector<model::ProjectLayerRecord>& layers = snapshot_->manifest.layers;
    if (top_first.size() != layers.size()) {
        error = QStringLiteral("The layer tree did not match the project.");
        return false;
    }
    std::map<std::string, const model::ProjectLayerRecord*> by_id;
    for (const model::ProjectLayerRecord& record : layers) {
        by_id.emplace(record.id, &record);
    }
    std::map<std::string, std::optional<std::string>> parent_of;
    for (const auto& [id, parent] : top_first) {
        if (by_id.find(id) == by_id.end() || (parent && by_id.find(*parent) == by_id.end())) {
            error = QStringLiteral("The layer tree did not match the project.");
            return false;
        }
        parent_of[id] = parent;
    }
    // Refuse a folder dropped inside itself: following the parent chain must reach the root.
    for (const auto& [id, parent] : parent_of) {
        std::string cursor = id;
        for (std::size_t step = 0; step <= parent_of.size(); ++step) {
            const auto found = parent_of.find(cursor);
            if (found == parent_of.end() || !found->second) {
                cursor.clear();
                break;
            }
            cursor = *found->second;
        }
        if (!cursor.empty()) {
            error = QStringLiteral("A folder cannot be placed inside itself.");
            return false;
        }
    }
    // Input is top-first; the manifest is bottom-first, and a folder's contents follow it.
    std::map<std::string, std::vector<std::string>> children;
    for (const auto& [id, parent] : top_first) {
        children[parent.value_or(std::string())].push_back(id);
    }
    std::vector<model::ProjectLayerRecord> ordered;
    ordered.reserve(layers.size());
    std::function<void(const std::string&)> walk = [&](const std::string& parent) {
        const auto found = children.find(parent);
        if (found == children.end()) {
            return;
        }
        for (auto it = found->second.rbegin(); it != found->second.rend(); ++it) {
            const model::ProjectLayerRecord* record = by_id.at(*it);
            ordered.push_back(*record);
            if (record->is_group_layer()) {
                walk(*it);
            }
        }
    };
    walk(std::string());
    if (ordered.size() != layers.size()) {
        error = QStringLiteral("The layer tree did not match the project.");
        return false;
    }
    for (model::ProjectLayerRecord& record : ordered) {
        record.parent_id = parent_of.at(record.id);
    }
    push_history();
    layers = std::move(ordered);
    release_detached_clipping();
    return recomposite(error);
}

}  // namespace compositor::appwin
