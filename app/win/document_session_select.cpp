#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

#include "compositor/io/image_codec.hpp"
#include "compositor/render/guided_matte.hpp"
#include "compositor/render/placement.hpp"
#include "compositor/render/selection.hpp"
#include "document_session.hpp"

// Selection and crop operations, split out so `document_session.cpp` stays within the size limit.

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

/// A document-sized selection of the pixels of a placed image that pass a threshold: a layer's opaque
/// pixels (alpha >=>=?128) or a mask's hidden pixels (gray < 128). macOS traces these into a path and
/// back; thresholding the raster directly gives the same coverage.
[[nodiscard]] render::Selection threshold_selection(const render::RgbaSurface& image,
                                                    const model::LayerTransform& transform, int document_width,
                                                    int document_height, bool use_alpha, bool darker_than_half) {
    render::Selection selection;
    selection.width = document_width;
    selection.height = document_height;
    selection.coverage.assign(static_cast<std::size_t>(document_width) * static_cast<std::size_t>(document_height), 0);
    selection.min_x = document_width;
    selection.min_y = document_height;
    selection.max_x = -1;
    selection.max_y = -1;
    for (int y = 0; y < document_height; ++y) {
        for (int x = 0; x < document_width; ++x) {
            double pixel_x = 0.0;
            double pixel_y = 0.0;
            if (!render::layer_pixel_at(transform, image.width(), image.height(), static_cast<double>(x) + 0.5,
                                        static_cast<double>(y) + 0.5, pixel_x, pixel_y)) {
                continue;
            }
            const int ix = static_cast<int>(std::floor(pixel_x));
            const int iy = static_cast<int>(std::floor(pixel_y));
            if (ix < 0 || iy < 0 || ix >= image.width() || iy >= image.height()) {
                continue;
            }
            const std::uint8_t* pixel = image.data() + image.offset(ix, iy);
            const std::uint8_t value = use_alpha ? pixel[3] : pixel[0];
            const bool inside = darker_than_half ? value < 128 : value >= 128;
            if (!inside) {
                continue;
            }
            selection.coverage[static_cast<std::size_t>(y) * static_cast<std::size_t>(document_width) +
                               static_cast<std::size_t>(x)] = 255;
            selection.min_x = std::min(selection.min_x, x);
            selection.min_y = std::min(selection.min_y, y);
            selection.max_x = std::max(selection.max_x, x);
            selection.max_y = std::max(selection.max_y, y);
        }
    }
    return selection;
}

}  // namespace

void DocumentSession::apply_selection(const render::Selection& incoming, render::CombineMode mode) {
    if (mode == render::CombineMode::replace || !selection_) {
        selection_ = incoming;
    } else {
        selection_ = render::combine(*selection_, incoming, mode);
    }
}

void DocumentSession::select_rect(const render::DocRect& rect, render::CombineMode mode) {
    if (snapshot_) {
        apply_selection(render::from_rect(rect, snapshot_->manifest.width, snapshot_->manifest.height), mode);
    }
}

void DocumentSession::select_ellipse(const render::DocRect& bounds, render::CombineMode mode) {
    if (snapshot_) {
        apply_selection(render::from_ellipse(bounds, snapshot_->manifest.width, snapshot_->manifest.height), mode);
    }
}

void DocumentSession::select_lasso(const std::vector<render::Point>& points, render::CombineMode mode) {
    if (snapshot_) {
        apply_selection(render::from_polygon(points, snapshot_->manifest.width, snapshot_->manifest.height), mode);
    }
}

void DocumentSession::clear_selection() {
    selection_.reset();
}

std::vector<std::vector<render::Point>> DocumentSession::selection_outline() const {
    if (!selection_) {
        return {};
    }
    return render::outline(*selection_);
}

void DocumentSession::select_all() {
    if (snapshot_) {
        selection_ = render::from_rect(render::DocRect{0.0, 0.0, static_cast<double>(snapshot_->manifest.width),
                                                       static_cast<double>(snapshot_->manifest.height)},
                                       snapshot_->manifest.width, snapshot_->manifest.height);
    }
}

void DocumentSession::invert_selection() {
    if (selection_) {
        selection_ = render::invert(*selection_);
    }
}

void DocumentSession::feather_selection(double radius) {
    if (selection_) {
        selection_ = render::feather(*selection_, radius);
    }
}

void DocumentSession::expand_selection(int radius) {
    if (selection_) {
        selection_ = render::expand(*selection_, radius);
    }
}

void DocumentSession::contract_selection(int radius) {
    if (selection_) {
        selection_ = render::contract(*selection_, radius);
    }
}

bool DocumentSession::select_wand(const std::string& id, double document_x, double document_y, int tolerance,
                                  bool contiguous, render::CombineMode mode, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    const auto image = snapshot_->images.find(id);
    if (record == nullptr || record->is_group_layer() || record->adjustment || image == snapshot_->images.end()) {
        error = QStringLiteral("Select a pixel layer for the magic wand.");
        return false;
    }
    try {
        const render::RgbaSurface surface = io::decode_image(image->second.png);
        apply_selection(render::magic_wand(surface, record->transform, snapshot_->manifest.width,
                                           snapshot_->manifest.height, document_x, document_y, tolerance, contiguous),
                        mode);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return true;
}

QColor DocumentSession::sample_color(double document_x, double document_y) const {
    if (flat_.empty()) {
        return QColor();
    }
    const int x = static_cast<int>(std::floor(document_x));
    const int y = static_cast<int>(std::floor(document_y));
    if (x < 0 || y < 0 || x >= flat_.width() || y >= flat_.height()) {
        return QColor();
    }
    const std::uint8_t* pixel = flat_.data() + flat_.offset(x, y);
    const int alpha = pixel[3];
    if (alpha == 0) {
        return QColor(0, 0, 0);
    }
    const auto unpremultiply = [alpha](std::uint8_t c) {
        return static_cast<int>(std::min(255, (static_cast<int>(c) * 255 + alpha / 2) / alpha));
    };
    return QColor(unpremultiply(pixel[0]), unpremultiply(pixel[1]), unpremultiply(pixel[2]));
}

bool DocumentSession::delete_selection(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (!has_selection()) {
        error = QStringLiteral("Make a selection first.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    const auto image = snapshot_->images.find(id);
    if (record == nullptr || record->is_group_layer() || record->adjustment || image == snapshot_->images.end()) {
        error = QStringLiteral("Select a pixel layer to edit.");
        return false;
    }
    try {
        ensure_surfaces();
        render::RgbaSurface& surface = image_surfaces_.at(id);
        push_history();
        render::clear_in_layer(surface, record->transform, *selection_);
        image->second.width = surface.width();
        image->second.height = surface.height();
        image->second.png = io::encode_png(surface);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

bool DocumentSession::fill_selection(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (!has_selection()) {
        error = QStringLiteral("Make a selection first.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    const auto image = snapshot_->images.find(id);
    if (record == nullptr || record->is_group_layer() || record->adjustment || image == snapshot_->images.end()) {
        error = QStringLiteral("Select a pixel layer to edit.");
        return false;
    }
    try {
        ensure_surfaces();
        render::RgbaSurface& surface = image_surfaces_.at(id);
        push_history();
        render::fill_in_layer(surface, record->transform, *selection_, brush_.red, brush_.green, brush_.blue,
                              brush_.opacity);
        image->second.width = surface.width();
        image->second.height = surface.height();
        image->second.png = io::encode_png(surface);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

bool DocumentSession::crop_to_selection(QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (!has_selection()) {
        error = QStringLiteral("Make a selection first.");
        return false;
    }
    const int x = std::max(0, selection_->min_x);
    const int y = std::max(0, selection_->min_y);
    const int width = std::min(selection_->max_x - selection_->min_x + 1, snapshot_->manifest.width - x);
    const int height = std::min(selection_->max_y - selection_->min_y + 1, snapshot_->manifest.height - y);
    if (width < 1 || height < 1) {
        error = QStringLiteral("The selection is outside the canvas.");
        return false;
    }
    push_history();
    for (model::ProjectLayerRecord& layer : snapshot_->manifest.layers) {
        layer.transform.origin_x -= x;
        layer.transform.origin_y -= y;
        if (layer.mask_placement) {
            layer.mask_placement->origin_x -= x;
            layer.mask_placement->origin_y -= y;
        }
    }
    if (snapshot_->manifest.guides) {
        for (model::CanvasGuide& guide : *snapshot_->manifest.guides) {
            guide.position -= guide.axis == model::CanvasGuide::Axis::vertical ? x : y;
        }
    }
    snapshot_->manifest.width = width;
    snapshot_->manifest.height = height;
    selection_.reset();
    return recomposite(error);
}

bool DocumentSession::load_layer_selection(const std::string& id, render::CombineMode mode, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const model::ProjectLayerRecord* record = layer(id);
    const auto image = snapshot_->images.find(id);
    if (record == nullptr || record->is_group_layer() || record->adjustment || image == snapshot_->images.end()) {
        error = QStringLiteral("That layer has no pixels to select.");
        return false;
    }
    try {
        const render::RgbaSurface surface = io::decode_image(image->second.png);
        const render::Selection loaded = threshold_selection(surface, record->transform, snapshot_->manifest.width,
                                                             snapshot_->manifest.height, true, false);
        if (loaded.empty()) {
            error = QStringLiteral("That layer has no opaque pixels.");
            return false;
        }
        apply_selection(loaded, mode);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return true;
}

bool DocumentSession::load_mask_selection(const std::string& id, render::CombineMode mode, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const model::ProjectLayerRecord* record = layer(id);
    const auto mask = snapshot_->masks.find(id);
    if (record == nullptr || mask == snapshot_->masks.end()) {
        error = QStringLiteral("That layer has no mask to select from.");
        return false;
    }
    try {
        const render::RgbaSurface surface = io::decode_image(mask->second.png);
        const model::LayerTransform transform = record->mask_placement.value_or(record->transform);
        const render::Selection loaded =
            threshold_selection(surface, transform, snapshot_->manifest.width, snapshot_->manifest.height, false, true);
        if (loaded.empty()) {
            error = QStringLiteral("That mask has no hidden pixels.");
            return false;
        }
        apply_selection(loaded, mode);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return true;
}

bool DocumentSession::color_range_select(std::uint8_t red, std::uint8_t green, std::uint8_t blue, int fuzziness,
                                         bool invert, render::CombineMode mode, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const std::vector<std::uint8_t> include{red, green, blue};
    const render::Selection loaded = render::color_range(flat_, include, {}, fuzziness, invert);
    if (loaded.empty()) {
        error = QStringLiteral("No pixels match that color range.");
        return false;
    }
    apply_selection(loaded, mode);
    return true;
}
}  // namespace compositor::appwin
