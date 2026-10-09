#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "compositor/io/image_codec.hpp"
#include "compositor/model/uuid.hpp"
#include "compositor/render/compositor.hpp"
#include "document_session.hpp"

// Layer transforms and flattening, split out so `document_session.cpp` stays within the size limit.

namespace compositor::appwin {
namespace {

[[nodiscard]] std::map<std::string, render::RgbaSurface> decode_all(
    const std::map<std::string, model::ImageAsset>& assets) {
    std::map<std::string, render::RgbaSurface> surfaces;
    for (const auto& [id, asset] : assets) {
        surfaces.emplace(id, io::decode_image(asset.png));
    }
    return surfaces;
}

[[nodiscard]] model::ProjectLayerRecord* find(model::ProjectManifest& manifest, const std::string& id) {
    for (model::ProjectLayerRecord& record : manifest.layers) {
        if (record.id == id) {
            return &record;
        }
    }
    return nullptr;
}

}  // namespace

bool DocumentSession::flip_layer(const std::string& id, bool horizontal, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    if (record == nullptr) {
        error = QStringLiteral("Layer not found.");
        return false;
    }
    push_history();
    if (horizontal) {
        record->transform.flip_x = !record->transform.flip_x;
    } else {
        record->transform.flip_y = !record->transform.flip_y;
    }
    return recomposite(error);
}

bool DocumentSession::rotate_layer_90(const std::string& id, bool clockwise, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    if (record == nullptr) {
        error = QStringLiteral("Layer not found.");
        return false;
    }
    push_history();
    double rotation = std::fmod(record->transform.rotation + (clockwise ? 90.0 : -90.0), 360.0);
    if (rotation > 180.0) {
        rotation -= 360.0;
    }
    if (rotation <= -180.0) {
        rotation += 360.0;
    }
    record->transform.rotation = rotation;
    return recomposite(error);
}

bool DocumentSession::reset_transform(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    if (record == nullptr) {
        error = QStringLiteral("Layer not found.");
        return false;
    }
    const auto image = snapshot_->images.find(id);
    const double width = image != snapshot_->images.end() ? image->second.width : snapshot_->manifest.width;
    const double height = image != snapshot_->images.end() ? image->second.height : snapshot_->manifest.height;
    push_history();
    model::LayerTransform& transform = record->transform;
    transform.rotation = 0.0;
    transform.flip_x = false;
    transform.flip_y = false;
    transform.width = width;
    transform.height = height;
    transform.origin_x = (snapshot_->manifest.width - width) / 2.0;
    transform.origin_y = (snapshot_->manifest.height - height) / 2.0;
    return recomposite(error);
}

bool DocumentSession::set_transform(const std::string& id, const model::LayerTransform& transform, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    if (record == nullptr) {
        error = QStringLiteral("Layer not found.");
        return false;
    }
    if (transform.width < 1.0 || transform.height < 1.0 || !transform.is_valid()) {
        error = QStringLiteral("That transform is not valid.");
        return false;
    }
    push_history();
    record->transform = transform;
    if (record->shape) {
        QString shape_error;
        redraw_shape_pixels(id, shape_error);
    }
    return recomposite(error);
}

bool DocumentSession::flatten(QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    render::RgbaSurface merged;
    try {
        merged = render::composite_document(snapshot_->manifest, decode_all(snapshot_->images),
                                            decode_all(snapshot_->masks));
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    push_history();
    model::ProjectLayerRecord background;
    background.id = model::generate_uuid();
    background.name = "Background";
    background.is_visible = true;
    background.transform.origin_x = 0.0;
    background.transform.origin_y = 0.0;
    background.transform.width = snapshot_->manifest.width;
    background.transform.height = snapshot_->manifest.height;
    background.image_file = background.id + ".png";
    try {
        model::ImageAsset asset;
        asset.width = merged.width();
        asset.height = merged.height();
        asset.png = io::encode_png(merged);
        const std::string id = background.id;
        snapshot_->manifest.layers.clear();
        snapshot_->manifest.layers.push_back(background);
        snapshot_->manifest.active_layer_id = id;
        snapshot_->images.clear();
        snapshot_->images[id] = std::move(asset);
        snapshot_->masks.clear();
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    invalidate_surfaces();
    return recomposite(error);
}

bool DocumentSession::set_blend_mode(const std::string& id, model::LayerBlendMode mode, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    for (model::ProjectLayerRecord& record : snapshot_->manifest.layers) {
        if (record.id != id) {
            continue;
        }
        if (record.is_group_layer()) {
            error = QStringLiteral("Folders are pass-through and stay Normal.");
            return false;
        }
        push_history();
        record.blend_mode = mode;
        return recomposite(error);
    }
    error = QStringLiteral("Layer not found.");
    return false;
}

bool DocumentSession::flip_canvas(bool horizontal, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    push_history();
    const double cx = snapshot_->manifest.width / 2.0;
    const double cy = snapshot_->manifest.height / 2.0;
    for (model::ProjectLayerRecord& layer : snapshot_->manifest.layers) {
        model::LayerTransform& transform = layer.transform;
        if (horizontal) {
            const double center = transform.center_x();
            transform.origin_x = (2.0 * cx - center) - transform.width / 2.0;
            transform.flip_x = !transform.flip_x;
        } else {
            const double center = transform.center_y();
            transform.origin_y = (2.0 * cy - center) - transform.height / 2.0;
            transform.flip_y = !transform.flip_y;
        }
        if (layer.mask_placement) {
            model::LayerTransform& placement = *layer.mask_placement;
            if (horizontal) {
                placement.origin_x = (2.0 * cx - placement.center_x()) - placement.width / 2.0;
                placement.flip_x = !placement.flip_x;
            } else {
                placement.origin_y = (2.0 * cy - placement.center_y()) - placement.height / 2.0;
                placement.flip_y = !placement.flip_y;
            }
        }
    }
    return recomposite(error);
}

bool DocumentSession::rename_layer(const std::string& id, const QString& name, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        error = QStringLiteral("A layer needs a name.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    if (record == nullptr) {
        error = QStringLiteral("Layer not found.");
        return false;
    }
    push_history();
    record->name = trimmed.toStdString();
    return recomposite(error);
}

bool DocumentSession::move_layer_order(const std::string& id, int delta, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (delta == 0) {
        return true;
    }
    std::vector<model::ProjectLayerRecord>& layers = snapshot_->manifest.layers;
    const auto found = std::find_if(layers.begin(), layers.end(),
                                    [&id](const model::ProjectLayerRecord& record) { return record.id == id; });
    if (found == layers.end()) {
        error = QStringLiteral("Layer not found.");
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(found - layers.begin());
    const std::optional<std::string>& parent = layers[index].parent_id;
    const int step = delta > 0 ? 1 : -1;
    for (long other = static_cast<long>(index) + step; other >= 0 && other < static_cast<long>(layers.size());
         other += step) {
        if (layers[static_cast<std::size_t>(other)].parent_id == parent) {
            push_history();
            std::swap(layers[index], layers[static_cast<std::size_t>(other)]);
            adopt_clipping(id);
            release_detached_clipping();
            return recomposite(error);
        }
    }
    return true;  // already at the end of its siblings
}

bool DocumentSession::add_folder(const std::string& active_id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    push_history();
    int number = 1;
    for (const model::ProjectLayerRecord& record : snapshot_->manifest.layers) {
        if (record.name == "Folder " + std::to_string(number)) {
            ++number;
        }
    }
    model::ProjectLayerRecord folder;
    folder.id = model::generate_uuid();
    folder.name = "Folder " + std::to_string(number);
    folder.is_visible = true;
    folder.is_group = true;
    folder.transform.origin_x = 0.0;
    folder.transform.origin_y = 0.0;
    folder.transform.width = snapshot_->manifest.width;
    folder.transform.height = snapshot_->manifest.height;
    const std::string id = folder.id;
    auto insert_at = snapshot_->manifest.layers.end();
    for (auto it = snapshot_->manifest.layers.begin(); it != snapshot_->manifest.layers.end(); ++it) {
        if (it->id == active_id) {
            insert_at = it + 1;
            break;
        }
    }
    snapshot_->manifest.layers.insert(insert_at, std::move(folder));
    snapshot_->manifest.active_layer_id = id;
    return recomposite(error);
}

QImage DocumentSession::layer_thumbnail(const std::string& id, int max_size) const {
    if (!snapshot_) {
        return {};
    }
    const render::RgbaSurface* surface = nullptr;
    const auto cached = image_surfaces_.find(id);
    if (cached != image_surfaces_.end()) {
        surface = &cached->second;
    }
    try {
        const render::RgbaSurface decoded =
            surface != nullptr ? render::RgbaSurface() : io::decode_image(snapshot_->images.at(id).png);
        const render::RgbaSurface& source = surface != nullptr ? *surface : decoded;
        const QImage image(source.data(), source.width(), source.height(), source.width() * 4,
                           QImage::Format_RGBA8888_Premultiplied);
        return image.scaled(max_size, max_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } catch (const std::exception&) {
        return {};
    }
}

}  // namespace compositor::appwin
