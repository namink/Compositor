#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <string>

#include "compositor/io/image_codec.hpp"
#include "compositor/model/limits.hpp"
#include "compositor/model/uuid.hpp"
#include "document_session.hpp"

// Shape layers, ported from the macOS app's `ShapeTool.swift`: a new layer whose pixels are the shape,
// with `shape` metadata kept so the shape stays editable and round-trips with the macOS app.

namespace compositor::appwin {
namespace {

[[nodiscard]] std::string next_shape_name(const model::ProjectManifest& manifest, render::ShapeKind kind) {
    const std::string base(render::shape_kind_name(kind));
    for (int number = 1;; ++number) {
        const std::string candidate = base + " " + std::to_string(number);
        bool taken = false;
        for (const model::ProjectLayerRecord& record : manifest.layers) {
            if (record.name == candidate) {
                taken = true;
                break;
            }
        }
        if (!taken) {
            return candidate;
        }
    }
}

[[nodiscard]] nlohmann::json shape_json(const render::ShapeStyle& style) {
    nlohmann::json shape;
    shape["kind"] = std::string(render::shape_kind_name(style.kind));
    shape["red"] = style.red;
    shape["green"] = style.green;
    shape["blue"] = style.blue;
    shape["cornerRadius"] = style.corner_radius;
    if (style.kind == render::ShapeKind::line) {
        shape["lineWidth"] = style.line_width;
        shape["start"] = {{"x", style.start_x}, {"y", style.start_y}};
        shape["end"] = {{"x", style.end_x}, {"y", style.end_y}};
    }
    return shape;
}

}  // namespace

bool DocumentSession::add_shape_layer(const render::ShapeStyle& style, double origin_x, double origin_y, int width,
                                      int height, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (width < 1 || height < 1 || width > model::DocumentLimits::kMaxSide ||
        height > model::DocumentLimits::kMaxSide ||
        static_cast<std::int64_t>(width) * height > model::DocumentLimits::kMaxSurfacePixels) {
        error = QStringLiteral("That shape is too large.");
        return false;
    }
    const render::RgbaSurface surface = render::draw_shape(style, width, height);
    push_history();
    model::ProjectLayerRecord record;
    record.id = model::generate_uuid();
    record.name = next_shape_name(snapshot_->manifest, style.kind);
    record.is_visible = true;
    record.transform.origin_x = origin_x;
    record.transform.origin_y = origin_y;
    record.transform.width = width;
    record.transform.height = height;
    record.image_file = record.id + ".png";
    record.shape = shape_json(style);
    const std::optional<std::string> active = snapshot_->manifest.active_layer_id;
    if (active) {
        for (const model::ProjectLayerRecord& other : snapshot_->manifest.layers) {
            if (other.id == *active) {
                record.parent_id = other.is_group_layer() ? other.id : other.parent_id;
                break;
            }
        }
    }
    try {
        model::ImageAsset asset;
        asset.width = width;
        asset.height = height;
        asset.png = io::encode_png(surface);
        snapshot_->images[record.id] = std::move(asset);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    const std::string new_id = record.id;
    auto insert_at = snapshot_->manifest.layers.end();
    if (active) {
        for (auto it = snapshot_->manifest.layers.begin(); it != snapshot_->manifest.layers.end(); ++it) {
            if (it->id == *active) {
                insert_at = it + 1;
                break;
            }
        }
    }
    snapshot_->manifest.layers.insert(insert_at, std::move(record));
    snapshot_->manifest.active_layer_id = new_id;
    invalidate_surfaces();
    return recomposite(error);
}

namespace {

[[nodiscard]] render::ShapeStyle style_from_json(const nlohmann::json& shape) {
    render::ShapeStyle style;
    const std::string kind = shape.value("kind", std::string("Rectangle"));
    style.kind = kind == "Ellipse" ? render::ShapeKind::ellipse
                 : kind == "Line"  ? render::ShapeKind::line
                                   : render::ShapeKind::rectangle;
    style.red = static_cast<float>(shape.value("red", 0.0));
    style.green = static_cast<float>(shape.value("green", 0.0));
    style.blue = static_cast<float>(shape.value("blue", 0.0));
    style.corner_radius = shape.value("cornerRadius", 0.0);
    style.line_width = shape.value("lineWidth", 0.0);
    if (shape.contains("start") && shape.contains("end")) {
        style.has_ends = true;
        style.start_x = shape["start"].value("x", 0.0);
        style.start_y = shape["start"].value("y", 0.0);
        style.end_x = shape["end"].value("x", 1.0);
        style.end_y = shape["end"].value("y", 1.0);
    }
    return style;
}

}  // namespace

bool DocumentSession::redraw_shape_pixels(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = nullptr;
    for (model::ProjectLayerRecord& candidate : snapshot_->manifest.layers) {
        if (candidate.id == id) {
            record = &candidate;
            break;
        }
    }
    if (record == nullptr || !record->shape) {
        return false;
    }
    const int width = std::max(1, static_cast<int>(std::lround(record->transform.width)));
    const int height = std::max(1, static_cast<int>(std::lround(record->transform.height)));
    const render::RgbaSurface surface = render::draw_shape(style_from_json(*record->shape), width, height);
    try {
        model::ImageAsset& asset = snapshot_->images[id];
        asset.width = width;
        asset.height = height;
        asset.png = io::encode_png(surface);
        image_surfaces_[id] = surface;
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return true;
}

bool DocumentSession::is_shape_layer(const std::string& id) const {
    const model::ProjectLayerRecord* record = layer(id);
    return record != nullptr && record->shape.has_value();
}

bool DocumentSession::update_shape_layer(const std::string& id, const render::ShapeStyle& style, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = nullptr;
    for (model::ProjectLayerRecord& candidate : snapshot_->manifest.layers) {
        if (candidate.id == id) {
            record = &candidate;
            break;
        }
    }
    if (record == nullptr || !record->shape) {
        error = QStringLiteral("Select a shape layer to edit.");
        return false;
    }
    int width = static_cast<int>(std::lround(record->transform.width));
    int height = static_cast<int>(std::lround(record->transform.height));
    if (width < 1 || height < 1) {
        const auto image = snapshot_->images.find(id);
        if (image == snapshot_->images.end()) {
            error = QStringLiteral("That shape layer has no pixels.");
            return false;
        }
        width = image->second.width;
        height = image->second.height;
    }
    const render::RgbaSurface surface = render::draw_shape(style, width, height);
    push_history();
    record->transform.width = width;
    record->transform.height = height;
    record->shape = shape_json(style);
    try {
        model::ImageAsset& asset = snapshot_->images[id];
        asset.width = width;
        asset.height = height;
        asset.png = io::encode_png(surface);
        image_surfaces_[id] = surface;
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

}  // namespace compositor::appwin
