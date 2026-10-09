#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include "compositor/io/image_codec.hpp"
#include "compositor/model/uuid.hpp"
#include "compositor/render/compositor.hpp"
#include "compositor/render/selection.hpp"
#include "document_session.hpp"

// Pixel copy / paste, ported from the macOS app's `SelectionClipboard.swift`: Copy takes the active
// layer's pixels through the selection (or the whole layer without one), Copy Merged flattens every
// visible layer through it, and Paste drops the pixels back where they came from as a new layer.

namespace compositor::appwin {
namespace {

[[nodiscard]] std::uint8_t scale(std::uint8_t value, int coverage) {
    return static_cast<std::uint8_t>((static_cast<int>(value) * coverage + 127) / 255);
}

}  // namespace

bool DocumentSession::render_selected_pixels(bool merged, render::RgbaSurface& surface, int& origin_x, int& origin_y,
                                             QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const std::optional<std::string> active = snapshot_->manifest.active_layer_id;
    if (!merged && !active) {
        error = QStringLiteral("Select a layer to copy.");
        return false;
    }
    const bool clipped = selection_ && !selection_->empty();
    if (merged && !clipped) {
        error = QStringLiteral("Select an area first.");
        return false;
    }

    const int canvas_width = snapshot_->manifest.width;
    const int canvas_height = snapshot_->manifest.height;
    int x0 = 0;
    int y0 = 0;
    int x1 = canvas_width;
    int y1 = canvas_height;
    if (clipped) {
        x0 = std::max(0, selection_->min_x);
        y0 = std::max(0, selection_->min_y);
        x1 = std::min(canvas_width, selection_->max_x + 1);
        y1 = std::min(canvas_height, selection_->max_y + 1);
    }
    if (x1 <= x0 || y1 <= y0) {
        error = QStringLiteral("There is nothing to copy.");
        return false;
    }

    render::RgbaSurface placed;
    try {
        if (merged) {
            placed = flat_;
        } else {
            model::ProjectManifest subset;
            subset.width = canvas_width;
            subset.height = canvas_height;
            std::map<std::string, render::RgbaSurface> images;
            std::map<std::string, render::RgbaSurface> masks;
            for (const model::ProjectLayerRecord& record : snapshot_->manifest.layers) {
                if (record.id != *active) {
                    continue;
                }
                subset.layers.push_back(record);
                const auto image = snapshot_->images.find(record.id);
                if (image != snapshot_->images.end()) {
                    images.emplace(record.id, io::decode_image(image->second.png));
                }
                const auto mask = snapshot_->masks.find(record.id);
                if (mask != snapshot_->masks.end()) {
                    masks.emplace(record.id, io::decode_image(mask->second.png));
                }
            }
            placed = render::composite_document(subset, images, masks);
        }
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }

    const int width = x1 - x0;
    const int height = y1 - y0;
    render::RgbaSurface result(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::uint8_t* pixel = placed.data() + placed.offset(x0 + x, y0 + y);
            if (clipped) {
                const int coverage = selection_->at(x0 + x, y0 + y);
                result.set(x, y, scale(pixel[0], coverage), scale(pixel[1], coverage), scale(pixel[2], coverage),
                           scale(pixel[3], coverage));
            } else {
                result.set(x, y, pixel[0], pixel[1], pixel[2], pixel[3]);
            }
        }
    }
    surface = std::move(result);
    origin_x = x0;
    origin_y = y0;
    return true;
}

QImage DocumentSession::pixel_clipboard_image() const {
    if (!pixel_clipboard_ || pixel_clipboard_->surface.empty()) {
        return {};
    }
    const render::RgbaSurface& surface = pixel_clipboard_->surface;
    return QImage(surface.data(), surface.width(), surface.height(), surface.width() * 4,
                  QImage::Format_RGBA8888_Premultiplied)
        .copy();
}

bool DocumentSession::copy_selection(QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (!has_selection()) {
        copy_layers();  // no selection copies the whole layer, as Photoshop does
        return true;
    }
    PixelClipboard clipboard;
    if (!render_selected_pixels(false, clipboard.surface, clipboard.origin_x, clipboard.origin_y, error)) {
        return false;
    }
    clipboard_ = std::nullopt;
    pixel_clipboard_ = std::move(clipboard);
    return true;
}

bool DocumentSession::copy_merged(QString& error) {
    PixelClipboard clipboard;
    if (!render_selected_pixels(true, clipboard.surface, clipboard.origin_x, clipboard.origin_y, error)) {
        return false;
    }
    clipboard_ = std::nullopt;
    pixel_clipboard_ = std::move(clipboard);
    return true;
}

bool DocumentSession::cut_selection(QString& error) {
    if (!snapshot_ || !has_selection()) {
        error = QStringLiteral("Select an area first.");
        return false;
    }
    const std::optional<std::string> active = snapshot_->manifest.active_layer_id;
    if (!active) {
        error = QStringLiteral("Select a layer to cut.");
        return false;
    }
    if (!copy_selection(error)) {
        return false;
    }
    ensure_surfaces();
    const auto surface = image_surfaces_.find(*active);
    if (surface == image_surfaces_.end()) {
        return true;  // nothing to clear (a folder or adjustment layer)
    }
    for (const model::ProjectLayerRecord& record : snapshot_->manifest.layers) {
        if (record.id == *active) {
            push_history();
            render::clear_in_layer(surface->second, record.transform, *selection_);
            break;
        }
    }
    model::ImageAsset& asset = snapshot_->images[*active];
    asset.png = io::encode_png(surface->second);
    invalidate_surfaces();
    return recomposite(error);
}

bool DocumentSession::paste(QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (pixel_clipboard_) {
        PixelClipboard clipboard = std::move(*pixel_clipboard_);
        pixel_clipboard_ = std::nullopt;
        return insert_pixel_layer(std::move(clipboard.surface), clipboard.origin_x, clipboard.origin_y, "Paste", error);
    }
    return paste_layers(error);
}

bool DocumentSession::layer_via_copy(QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (!has_selection()) {
        const std::optional<std::string> active = snapshot_->manifest.active_layer_id;
        if (!active) {
            error = QStringLiteral("Select a layer first.");
            return false;
        }
        return duplicate_layer(*active, error);
    }
    render::RgbaSurface surface;
    int origin_x = 0;
    int origin_y = 0;
    if (!render_selected_pixels(false, surface, origin_x, origin_y, error)) {
        return false;
    }
    return insert_pixel_layer(std::move(surface), origin_x, origin_y, "Layer via Copy", error);
}

bool DocumentSession::float_selection(QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (!has_selection()) {
        error = QStringLiteral("Select an area first.");
        return false;
    }
    const std::optional<std::string> active = snapshot_->manifest.active_layer_id;
    if (!active) {
        error = QStringLiteral("Select a layer first.");
        return false;
    }
    const model::ProjectLayerRecord* record = layer(*active);
    if (record == nullptr || record->is_group_layer() || record->adjustment) {
        error = QStringLiteral("Select a pixel layer to float.");
        return false;
    }
    render::RgbaSurface surface;
    int origin_x = 0;
    int origin_y = 0;
    if (!render_selected_pixels(false, surface, origin_x, origin_y, error)) {
        return false;
    }
    ensure_surfaces();
    const auto source = image_surfaces_.find(*active);
    if (source == image_surfaces_.end()) {
        error = QStringLiteral("That layer has no pixels to float.");
        return false;
    }
    push_history();
    render::clear_in_layer(source->second, record->transform, *selection_);
    snapshot_->images[*active].png = io::encode_png(source->second);

    model::ProjectLayerRecord floating;
    floating.id = model::generate_uuid();
    floating.name = "Floating Selection";
    floating.is_visible = true;
    floating.transform.origin_x = origin_x;
    floating.transform.origin_y = origin_y;
    floating.transform.width = surface.width();
    floating.transform.height = surface.height();
    floating.image_file = floating.id + ".png";
    floating.parent_id = record->parent_id;
    try {
        model::ImageAsset asset;
        asset.width = surface.width();
        asset.height = surface.height();
        asset.png = io::encode_png(surface);
        snapshot_->images[floating.id] = std::move(asset);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    const std::string new_id = floating.id;
    auto insert_at = snapshot_->manifest.layers.end();
    for (auto it = snapshot_->manifest.layers.begin(); it != snapshot_->manifest.layers.end(); ++it) {
        if (it->id == *active) {
            insert_at = it + 1;
            break;
        }
    }
    snapshot_->manifest.layers.insert(insert_at, std::move(floating));
    snapshot_->manifest.active_layer_id = new_id;
    selection_.reset();
    invalidate_surfaces();
    return recomposite(error);
}

bool DocumentSession::insert_pixel_layer(render::RgbaSurface surface, int origin_x, int origin_y, const char* name,
                                         QString& error) {
    if (!snapshot_ || surface.empty()) {
        error = QStringLiteral("There is nothing to paste.");
        return false;
    }
    push_history();
    model::ProjectLayerRecord record;
    record.id = model::generate_uuid();
    record.name = name;
    record.is_visible = true;
    record.transform.origin_x = origin_x;
    record.transform.origin_y = origin_y;
    record.transform.width = surface.width();
    record.transform.height = surface.height();
    record.image_file = record.id + ".png";
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
        asset.width = surface.width();
        asset.height = surface.height();
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
    selection_.reset();  // Pasting drops the selection, as in Photoshop
    invalidate_surfaces();
    return recomposite(error);
}

}  // namespace compositor::appwin
