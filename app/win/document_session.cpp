#include "document_session.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>

#include "compositor/io/image_codec.hpp"
#include "compositor/model/uuid.hpp"
#include "compositor/render/compositor.hpp"
#include "compositor/render/paint.hpp"
#include "compositor/render/placement.hpp"

namespace compositor::appwin {
namespace {

namespace fs = std::filesystem;

[[nodiscard]] fs::path to_fs(const QString& path) {
    return fs::path(path.toStdWString());
}

[[nodiscard]] QImage to_qimage(const render::RgbaSurface& surface) {
    // RgbaSurface is premultiplied 8-bit RGBA, top row first 鈥?exactly Format_RGBA8888_Premultiplied.
    return QImage(surface.data(), surface.width(), surface.height(), surface.width() * 4,
                  QImage::Format_RGBA8888_Premultiplied)
        .copy();
}

}  // namespace

const model::ProjectManifest* DocumentSession::manifest() const {
    return snapshot_ ? &snapshot_->manifest : nullptr;
}

bool DocumentSession::open(const QString& path, QString& error) {
    try {
        snapshot_ = model::ProjectStore::load(to_fs(path));
        path_ = path.toStdString();
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    undo_history_.clear();
    redo_history_.clear();
    undo_names_.clear();
    redo_names_.clear();
    pending_edit_name_ = "Edit";
    stroking_ = false;
    invalidate_surfaces();
    return recomposite(error);
}

void DocumentSession::invalidate_surfaces() {
    image_surfaces_.clear();
    mask_surfaces_.clear();
}

void DocumentSession::ensure_surfaces() {
    if (!snapshot_) {
        invalidate_surfaces();
        return;
    }
    const auto sync = [](std::map<std::string, render::RgbaSurface>& cache,
                         const std::map<std::string, model::ImageAsset>& assets) {
        for (auto it = cache.begin(); it != cache.end();) {
            if (assets.count(it->first) == 0) {
                it = cache.erase(it);
            } else {
                ++it;
            }
        }
        for (const auto& [id, asset] : assets) {
            if (cache.count(id) == 0) {
                cache.emplace(id, io::decode_image(asset.png));
            }
        }
    };
    sync(image_surfaces_, snapshot_->images);
    sync(mask_surfaces_, snapshot_->masks);
}

bool DocumentSession::recomposite(QString& error) {
    try {
        ensure_surfaces();
        flat_ = render::composite_document(snapshot_->manifest, image_surfaces_, mask_surfaces_);
        image_ = to_qimage(flat_);
        return true;
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
}

bool DocumentSession::set_visibility(const std::string& id, bool visible, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    for (model::ProjectLayerRecord& layer : snapshot_->manifest.layers) {
        if (layer.id == id) {
            push_history();
            layer.is_visible = visible;
            return recomposite(error);
        }
    }
    error = QStringLiteral("Layer not found.");
    return false;
}

const model::ProjectLayerRecord* DocumentSession::layer(const std::string& id) const {
    if (!snapshot_) {
        return nullptr;
    }
    for (const model::ProjectLayerRecord& record : snapshot_->manifest.layers) {
        if (record.id == id) {
            return &record;
        }
    }
    return nullptr;
}

bool DocumentSession::move_layer(const std::string& id, double dx, double dy, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    for (model::ProjectLayerRecord& record : snapshot_->manifest.layers) {
        if (record.id == id) {
            record.transform.origin_x += dx;
            record.transform.origin_y += dy;
            return recomposite(error);
        }
    }
    error = QStringLiteral("Layer not found.");
    return false;
}

bool DocumentSession::set_layer_origin(const std::string& id, double origin_x, double origin_y, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    for (model::ProjectLayerRecord& record : snapshot_->manifest.layers) {
        if (record.id == id) {
            record.transform.origin_x = origin_x;
            record.transform.origin_y = origin_y;
            return recomposite(error);
        }
    }
    error = QStringLiteral("Layer not found.");
    return false;
}

bool DocumentSession::set_opacity(const std::string& id, double opacity, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    for (model::ProjectLayerRecord& record : snapshot_->manifest.layers) {
        if (record.id == id) {
            record.opacity = std::clamp(opacity, 0.0, 1.0);
            return recomposite(error);
        }
    }
    error = QStringLiteral("Layer not found.");
    return false;
}

void DocumentSession::set_brush(const BrushSettings& brush) {
    brush_ = brush;
}

void DocumentSession::push_history() {
    if (!snapshot_) {
        return;
    }
    undo_history_.push_back(*snapshot_);
    undo_names_.push_back(pending_edit_name_);
    pending_edit_name_ = "Edit";
    if (undo_history_.size() > 50) {
        undo_history_.erase(undo_history_.begin());
        undo_names_.erase(undo_names_.begin());
    }
    redo_history_.clear();
    redo_names_.clear();
}

void DocumentSession::begin_interaction() {
    push_history();
}

bool DocumentSession::undo(QString& error) {
    if (undo_history_.empty() || !snapshot_) {
        return false;
    }
    redo_history_.push_back(*snapshot_);
    redo_names_.push_back(undo_names_.back());
    undo_names_.pop_back();
    *snapshot_ = undo_history_.back();
    undo_history_.pop_back();
    invalidate_surfaces();
    return recomposite(error);
}

bool DocumentSession::redo(QString& error) {
    if (redo_history_.empty() || !snapshot_) {
        return false;
    }
    undo_history_.push_back(*snapshot_);
    undo_names_.push_back(redo_names_.back());
    redo_names_.pop_back();
    *snapshot_ = redo_history_.back();
    redo_history_.pop_back();
    invalidate_surfaces();
    return recomposite(error);
}

bool DocumentSession::add_blank_layer(const std::string& active_id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    push_history();
    model::ProjectLayerRecord record;
    record.id = model::generate_uuid();
    record.name = "Layer";
    record.is_visible = true;
    record.transform.origin_x = 0.0;
    record.transform.origin_y = 0.0;
    record.transform.width = snapshot_->manifest.width;
    record.transform.height = snapshot_->manifest.height;
    record.image_file = record.id + ".png";
    try {
        render::RgbaSurface blank(snapshot_->manifest.width, snapshot_->manifest.height);
        model::ImageAsset asset;
        asset.width = blank.width();
        asset.height = blank.height();
        asset.png = io::encode_png(blank);
        snapshot_->images[record.id] = std::move(asset);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    const std::string new_id = record.id;
    auto insert_at = snapshot_->manifest.layers.end();
    if (!active_id.empty()) {
        for (auto it = snapshot_->manifest.layers.begin(); it != snapshot_->manifest.layers.end(); ++it) {
            if (it->id == active_id) {
                insert_at = it + 1;
                break;
            }
        }
    }
    snapshot_->manifest.layers.insert(insert_at, std::move(record));
    snapshot_->manifest.active_layer_id = new_id;
    return recomposite(error);
}

bool DocumentSession::duplicate_layer(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    for (auto it = snapshot_->manifest.layers.begin(); it != snapshot_->manifest.layers.end(); ++it) {
        if (it->id != id) {
            continue;
        }
        push_history();
        model::ProjectLayerRecord copy = *it;
        copy.id = model::generate_uuid();
        copy.name = it->name + " copy";
        if (copy.image_file) {
            copy.image_file = copy.id + ".png";
        }
        if (copy.mask_file) {
            copy.mask_file = copy.id + ".mask.png";
        }
        copy.mask_source_id.reset();
        const std::string new_id = copy.id;
        const auto image = snapshot_->images.find(id);
        if (image != snapshot_->images.end()) {
            snapshot_->images[new_id] = image->second;
        }
        const auto mask = snapshot_->masks.find(id);
        if (mask != snapshot_->masks.end()) {
            snapshot_->masks[new_id] = mask->second;
        }
        snapshot_->manifest.layers.insert(it + 1, std::move(copy));
        return recomposite(error);
    }
    error = QStringLiteral("Layer not found.");
    return false;
}

bool DocumentSession::delete_layer(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    auto& layers = snapshot_->manifest.layers;
    const auto found = std::find_if(layers.begin(), layers.end(),
                                    [&id](const model::ProjectLayerRecord& record) { return record.id == id; });
    if (found == layers.end()) {
        error = QStringLiteral("Layer not found.");
        return false;
    }
    push_history();
    // Free any clipping links that pointed at the deleted layer.
    for (model::ProjectLayerRecord& record : layers) {
        if (record.mask_source_id == id) {
            record.mask_source_id.reset();
        }
    }
    layers.erase(found);
    snapshot_->images.erase(id);
    snapshot_->masks.erase(id);
    if (snapshot_->manifest.active_layer_id == id) {
        snapshot_->manifest.active_layer_id.reset();
    }
    return recomposite(error);
}

bool DocumentSession::save(const QString& path, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    try {
        model::ProjectStore::save(*snapshot_, to_fs(path));
        path_ = path.toStdString();
        return true;
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
}

bool DocumentSession::reload(QString& error) {
    if (path_.empty()) {
        error = QStringLiteral("This project has no file on disk yet.");
        return false;
    }
    const QString path = QString::fromStdString(path_);
    try {
        snapshot_ = model::ProjectStore::load(to_fs(path));
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    undo_history_.clear();
    redo_history_.clear();
    undo_names_.clear();
    redo_names_.clear();
    pending_edit_name_ = "Edit";
    invalidate_surfaces();
    return recomposite(error);
}

bool DocumentSession::export_png(const QString& path, QString& error) {
    if (flat_.empty()) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    try {
        const std::vector<std::uint8_t> bytes = io::encode_png(flat_);
        std::ofstream output(to_fs(path), std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!output) {
            throw std::runtime_error("Could not write the PNG file.");
        }
        return true;
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
}

bool DocumentSession::export_jpeg(const QString& path, int quality, QString& error) {
    if (flat_.empty()) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    try {
        const std::vector<std::uint8_t> bytes = io::encode_jpeg(flat_, quality);
        std::ofstream output(to_fs(path), std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!output) {
            throw std::runtime_error("Could not write the JPEG file.");
        }
        return true;
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
}

}  // namespace compositor::appwin
