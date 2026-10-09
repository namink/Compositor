#include <string>
#include <vector>

#include "document_session.hpp"

// The names and jumping behind the History panel, split out so `document_session.cpp` stays within
// the soft size limit.

namespace compositor::appwin {

std::vector<std::string> DocumentSession::history_labels() const {
    std::vector<std::string> labels;
    labels.reserve(undo_history_.size() + redo_history_.size() + 1);
    labels.emplace_back("Open");
    for (const std::string& name : undo_names_) {
        labels.push_back(name);
    }
    for (auto it = redo_names_.rbegin(); it != redo_names_.rend(); ++it) {
        labels.push_back(*it);
    }
    return labels;
}

QString DocumentSession::digest() const {
    if (!snapshot_) {
        return QStringLiteral("No project is open.");
    }
    const model::ProjectManifest& manifest = snapshot_->manifest;
    int groups = 0;
    int adjustments = 0;
    int masks = 0;
    int clipped = 0;
    int effects = 0;
    int text = 0;
    int shapes = 0;
    for (const model::ProjectLayerRecord& record : manifest.layers) {
        groups += record.is_group_layer() ? 1 : 0;
        adjustments += record.adjustment ? 1 : 0;
        masks += record.mask_file ? 1 : 0;
        clipped += record.mask_source_id ? 1 : 0;
        effects += record.effects ? 1 : 0;
        text += record.text ? 1 : 0;
        shapes += record.shape ? 1 : 0;
    }
    return QStringLiteral(
               "Document %1\nCanvas: %2 x %3 px\nLayers: %4 (%5 folders %6 adjustments)\n"
               "Masks: %7   Clipping links: %8\nEffects: %9   Text: %10   Shapes: %11\n"
               "Guides: %12")
        .arg(QString::fromStdString(manifest.document_id))
        .arg(manifest.width)
        .arg(manifest.height)
        .arg(manifest.layers.size())
        .arg(groups)
        .arg(adjustments)
        .arg(masks)
        .arg(clipped)
        .arg(effects)
        .arg(text)
        .arg(shapes)
        .arg(manifest.guides ? static_cast<int>(manifest.guides->size()) : 0);
}

bool DocumentSession::jump_history(int state, QString& error) {
    if (state < 0 || state > history_count()) {
        return false;
    }
    while (history_current() > state) {
        if (!undo(error)) {
            return false;
        }
    }
    while (history_current() < state) {
        if (!redo(error)) {
            return false;
        }
    }
    return true;
}

}  // namespace compositor::appwin
