#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "compositor/model/uuid.hpp"
#include "document_session.hpp"

// Copy / Paste of whole layers, ported from the macOS app's `SelectionClipboard.swift`. Copy takes the
// active layer and everything a folder holds; Paste brings it back — in this project or another open
// one — as new layers above the active layer, with parent and clipping links remapped to the copies.

namespace compositor::appwin {
namespace {

[[nodiscard]] std::set<std::string> subtree(const model::ProjectManifest& manifest, const std::string& id) {
    std::set<std::string> result{id};
    std::vector<std::string> pending{id};
    while (!pending.empty()) {
        const std::string parent = pending.back();
        pending.pop_back();
        for (const model::ProjectLayerRecord& record : manifest.layers) {
            if (record.parent_id && *record.parent_id == parent && result.insert(record.id).second) {
                pending.push_back(record.id);
            }
        }
    }
    return result;
}

}  // namespace

void DocumentSession::copy_layers() {
    if (!snapshot_) {
        return;
    }
    const std::optional<std::string> active = snapshot_->manifest.active_layer_id;
    if (!active) {
        return;
    }
    const std::set<std::string> included = subtree(snapshot_->manifest, *active);
    Clipboard clipboard;
    for (const model::ProjectLayerRecord& record : snapshot_->manifest.layers) {
        if (included.count(record.id) == 0) {
            continue;
        }
        clipboard.layers.push_back(record);
        const auto image = snapshot_->images.find(record.id);
        if (image != snapshot_->images.end()) {
            clipboard.images.emplace(record.id, image->second);
        }
        const auto mask = snapshot_->masks.find(record.id);
        if (mask != snapshot_->masks.end()) {
            clipboard.masks.emplace(record.id, mask->second);
        }
    }
    clipboard_ = std::move(clipboard);
}

bool DocumentSession::paste_layers(QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (!clipboard_ || clipboard_->layers.empty()) {
        error = QStringLiteral("There is nothing to paste.");
        return false;
    }
    const std::string root = clipboard_->layers.front().id;  // the copied active layer
    if (snapshot_->manifest.layers.size() + clipboard_->layers.size() > 10000) {
        error = QStringLiteral("The project would have too many layers.");
        return false;
    }

    std::map<std::string, std::string> mapping;
    for (const model::ProjectLayerRecord& record : clipboard_->layers) {
        mapping.emplace(record.id, model::generate_uuid());
    }

    push_history();
    std::vector<model::ProjectLayerRecord> copies;
    copies.reserve(clipboard_->layers.size());
    for (const model::ProjectLayerRecord& record : clipboard_->layers) {
        model::ProjectLayerRecord copy = record;
        copy.id = mapping.at(record.id);
        if (record.parent_id) {
            const auto parent = mapping.find(*record.parent_id);
            copy.parent_id = parent != mapping.end() ? std::optional<std::string>(parent->second) : record.parent_id;
        }
        if (record.mask_source_id) {
            const auto source = mapping.find(*record.mask_source_id);
            copy.mask_source_id =
                source != mapping.end() ? std::optional<std::string>(source->second) : record.mask_source_id;
        }
        if (record.id == root) {
            copy.name += " copy";
        }
        const auto image = clipboard_->images.find(record.id);
        if (image != clipboard_->images.end()) {
            snapshot_->images[copy.id] = image->second;
        }
        const auto mask = clipboard_->masks.find(record.id);
        if (mask != clipboard_->masks.end()) {
            snapshot_->masks[copy.id] = mask->second;
        }
        copies.push_back(std::move(copy));
    }

    // Land above the active layer (inside its folder), as Photoshop's Paste does.
    std::size_t index = snapshot_->manifest.layers.size();
    if (snapshot_->manifest.active_layer_id) {
        for (std::size_t i = 0; i < snapshot_->manifest.layers.size(); ++i) {
            if (snapshot_->manifest.layers[i].id == *snapshot_->manifest.active_layer_id) {
                index = i + 1;
                break;
            }
        }
    }
    snapshot_->manifest.layers.insert(snapshot_->manifest.layers.begin() + static_cast<std::ptrdiff_t>(index),
                                      copies.begin(), copies.end());
    snapshot_->manifest.active_layer_id = copies.front().id;
    invalidate_surfaces();
    return recomposite(error);
}

}  // namespace compositor::appwin
