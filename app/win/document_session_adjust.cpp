#include <optional>
#include <string>

#include "compositor/io/image_codec.hpp"
#include "compositor/model/uuid.hpp"
#include "compositor/render/adjustment.hpp"
#include "document_session.hpp"

// Adjustment layers and destructive filters, split out so `document_session.cpp` stays small.

namespace compositor::appwin {
namespace {

[[nodiscard]] model::ProjectLayerRecord* find(model::ProjectManifest& manifest, const std::string& id) {
    for (model::ProjectLayerRecord& record : manifest.layers) {
        if (record.id == id) {
            return &record;
        }
    }
    return nullptr;
}

}  // namespace

bool DocumentSession::add_adjustment_layer(const std::string& active_id, const std::string& kind, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    push_history();
    model::ProjectLayerRecord record;
    record.id = model::generate_uuid();
    record.name = kind;
    record.is_visible = true;
    record.transform.origin_x = 0.0;
    record.transform.origin_y = 0.0;
    record.transform.width = snapshot_->manifest.width;
    record.transform.height = snapshot_->manifest.height;
    record.adjustment = nlohmann::json{{"kind", kind}};
    const std::string id = record.id;
    auto insert_at = snapshot_->manifest.layers.end();
    for (auto it = snapshot_->manifest.layers.begin(); it != snapshot_->manifest.layers.end(); ++it) {
        if (it->id == active_id) {
            insert_at = it + 1;
            break;
        }
    }
    snapshot_->manifest.layers.insert(insert_at, std::move(record));
    snapshot_->manifest.active_layer_id = id;
    return recomposite(error);
}

bool DocumentSession::auto_levels(int mode, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const std::optional<std::string> active = snapshot_->manifest.active_layer_id;
    if (!active) {
        error = QStringLiteral("Select a layer first.");
        return false;
    }
    ensure_surfaces();
    const auto surface = image_surfaces_.find(*active);
    if (surface == image_surfaces_.end()) {
        error = QStringLiteral("Select a pixel layer first.");
        return false;
    }
    const nlohmann::json levels = render::auto_levels_settings(surface->second, mode);
    push_history();
    model::ProjectLayerRecord record;
    record.id = model::generate_uuid();
    record.name = "Levels";
    record.is_visible = true;
    record.transform.origin_x = 0.0;
    record.transform.origin_y = 0.0;
    record.transform.width = snapshot_->manifest.width;
    record.transform.height = snapshot_->manifest.height;
    record.adjustment = nlohmann::json{{"kind", "Levels"}, {"levels", levels}};
    const std::string id = record.id;
    auto insert_at = snapshot_->manifest.layers.end();
    for (auto it = snapshot_->manifest.layers.begin(); it != snapshot_->manifest.layers.end(); ++it) {
        if (it->id == *active) {
            insert_at = it + 1;
            break;
        }
    }
    snapshot_->manifest.layers.insert(insert_at, std::move(record));
    snapshot_->manifest.active_layer_id = id;
    return recomposite(error);
}

bool DocumentSession::update_adjustment(const std::string& id, const nlohmann::json& adjustment, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    if (record == nullptr || !record->adjustment) {
        error = QStringLiteral("That layer is not an adjustment.");
        return false;
    }
    push_history();
    record->adjustment = adjustment;
    return recomposite(error);
}

bool DocumentSession::begin_filter_preview(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const auto image = snapshot_->images.find(id);
    if (image == snapshot_->images.end()) {
        error = QStringLiteral("Select a pixel layer to filter.");
        return false;
    }
    ensure_surfaces();
    const auto surface = image_surfaces_.find(id);
    if (surface == image_surfaces_.end()) {
        error = QStringLiteral("Select a pixel layer to filter.");
        return false;
    }
    preview_original_ = surface->second;
    preview_layer_ = id;
    preview_active_ = true;
    return true;
}

bool DocumentSession::update_filter_preview(const nlohmann::json& adjustment, QString& error) {
    if (!preview_active_) {
        error = QStringLiteral("No preview is running.");
        return false;
    }
    try {
        render::RgbaSurface surface = preview_original_;
        render::apply_adjustment(adjustment, surface, 1.0, nullptr);
        image_surfaces_[preview_layer_] = std::move(surface);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

bool DocumentSession::commit_filter_preview(QString& error) {
    if (!preview_active_) {
        return false;
    }
    push_history();
    try {
        model::ImageAsset& asset = snapshot_->images[preview_layer_];
        const render::RgbaSurface& surface = image_surfaces_.at(preview_layer_);
        asset.width = surface.width();
        asset.height = surface.height();
        asset.png = io::encode_png(surface);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        preview_active_ = false;
        return false;
    }
    preview_active_ = false;
    return recomposite(error);
}

void DocumentSession::cancel_filter_preview() {
    if (!preview_active_) {
        return;
    }
    image_surfaces_[preview_layer_] = preview_original_;
    preview_active_ = false;
    QString error;
    recomposite(error);
}

bool DocumentSession::set_effects(const std::string& id, const nlohmann::json& effects, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    if (record == nullptr || record->is_group_layer() || record->adjustment) {
        error = QStringLiteral("Select a pixel layer to add effects to.");
        return false;
    }
    push_history();
    if (effects.is_object() && !effects.empty()) {
        record->effects = effects;
    } else {
        record->effects.reset();
    }
    return recomposite(error);
}

bool DocumentSession::apply_filter(const std::string& id, const nlohmann::json& adjustment, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    const auto image = snapshot_->images.find(id);
    if (record == nullptr || record->is_group_layer() || record->adjustment || image == snapshot_->images.end()) {
        error = QStringLiteral("Select a pixel layer to filter.");
        return false;
    }
    try {
        ensure_surfaces();
        render::RgbaSurface& surface = image_surfaces_.at(id);
        push_history();
        render::apply_adjustment(adjustment, surface, 1.0, nullptr);
        image->second.width = surface.width();
        image->second.height = surface.height();
        image->second.png = io::encode_png(surface);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

}  // namespace compositor::appwin
