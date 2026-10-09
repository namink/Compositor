#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "compositor/io/image_codec.hpp"
#include "compositor/render/paint.hpp"
#include "compositor/render/placement.hpp"
#include "document_session.hpp"

// Brush strokes, split out so `document_session.cpp` stays within the size limit.
//
// A stroke edits the cached surface directly and re-composites from memory; the PNG is encoded once,
// on `stroke_end`. Encoding or decoding a full layer on every mouse move is what made painting crawl.

namespace compositor::appwin {

namespace {

/// Paint one brush dab: blur for the Blur tool, otherwise a stamp (paint or erase).
void apply_brush(const BrushSettings& brush, render::RgbaSurface& surface, double px, double py) {
    if (brush.blur) {
        render::blur_dab(surface, px, py, brush.radius, brush.hardness, brush.radius * 0.35, brush.opacity);
        return;
    }
    render::BrushDab dab;
    dab.x = px;
    dab.y = py;
    dab.radius = brush.radius;
    dab.hardness = brush.hardness;
    dab.opacity = brush.opacity;
    dab.red = brush.red;
    dab.green = brush.green;
    dab.blue = brush.blue;
    dab.erase = brush.erase;
    render::stamp_dab(surface, dab);
}

/// Stamp a soft round brush's coverage into a gray buffer (255 where the brush paints).
void stamp_coverage(std::vector<std::uint8_t>& coverage, int width, int height, double center_x, double center_y,
                    double radius, double hardness) {
    if (radius <= 0.0) {
        return;
    }
    const double hard = std::clamp(hardness, 0.0, 0.999);
    const int min_x = std::max(0, static_cast<int>(std::floor(center_x - radius)));
    const int max_x = std::min(width - 1, static_cast<int>(std::ceil(center_x + radius)));
    const int min_y = std::max(0, static_cast<int>(std::floor(center_y - radius)));
    const int max_y = std::min(height - 1, static_cast<int>(std::ceil(center_y + radius)));
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const double distance = std::hypot(x + 0.5 - center_x, y + 0.5 - center_y);
            double value = 0.0;
            if (distance <= hard * radius) {
                value = 1.0;
            } else if (distance < radius) {
                const double t = std::clamp((radius - distance) / (radius * (1.0 - hard)), 0.0, 1.0);
                value = t * t * (3.0 - 2.0 * t);
            }
            const auto byte = static_cast<std::uint8_t>(std::lround(value * 255.0));
            std::uint8_t& slot = coverage[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)];
            slot = std::max(slot, byte);
        }
    }
}

}  // namespace

bool DocumentSession::heal_begin(const std::string& id, double document_x, double document_y, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const model::ProjectLayerRecord* record = layer(id);
    if (record == nullptr || record->is_group_layer() || record->adjustment || !record->image_file) {
        error = QStringLiteral("Select a pixel layer to heal.");
        return false;
    }
    ensure_surfaces();
    const auto image = image_surfaces_.find(id);
    if (image == image_surfaces_.end()) {
        error = QStringLiteral("This layer has no pixels.");
        return false;
    }
    push_history();
    heal_coverage_.assign(static_cast<std::size_t>(image->second.width()) * image->second.height(), 0U);
    heal_layer_ = id;
    healing_ = true;
    double px = 0.0;
    double py = 0.0;
    if (render::layer_pixel_at(record->transform, image->second.width(), image->second.height(), document_x, document_y,
                               px, py)) {
        heal_last_x_ = px;
        heal_last_y_ = py;
        stamp_coverage(heal_coverage_, image->second.width(), image->second.height(), px, py, brush_.radius,
                       brush_.hardness);
    }
    return true;
}

void DocumentSession::heal_move(double document_x, double document_y) {
    if (!healing_) {
        return;
    }
    const auto image = image_surfaces_.find(heal_layer_);
    const model::ProjectLayerRecord* record = layer(heal_layer_);
    if (image == image_surfaces_.end() || record == nullptr) {
        return;
    }
    double px = 0.0;
    double py = 0.0;
    if (!render::layer_pixel_at(record->transform, image->second.width(), image->second.height(), document_x,
                                document_y, px, py)) {
        return;
    }
    const double distance = std::hypot(px - heal_last_x_, py - heal_last_y_);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / std::max(1.0, brush_.radius * 0.25))));
    for (int i = 0; i <= steps; ++i) {
        const double t = static_cast<double>(i) / steps;
        stamp_coverage(heal_coverage_, image->second.width(), image->second.height(),
                       heal_last_x_ + (px - heal_last_x_) * t, heal_last_y_ + (py - heal_last_y_) * t, brush_.radius,
                       brush_.hardness);
    }
    heal_last_x_ = px;
    heal_last_y_ = py;
}

void DocumentSession::heal_end() {
    if (!healing_ || !snapshot_) {
        return;
    }
    healing_ = false;
    const auto image = image_surfaces_.find(heal_layer_);
    if (image == image_surfaces_.end() || image->second.empty()) {
        return;
    }
    const int width = image->second.width();
    const int height = image->second.height();
    render::spot_heal(image->second, heal_coverage_, 1.0, 0, 0U);
    auto& asset = snapshot_->images[heal_layer_];
    asset.width = width;
    asset.height = height;
    asset.png = io::encode_png(image->second);
    QString error;
    recomposite(error);
    heal_coverage_.clear();
}

bool DocumentSession::clone_set_source(const std::string& id, double document_x, double document_y, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const model::ProjectLayerRecord* record = layer(id);
    ensure_surfaces();
    const auto image = image_surfaces_.find(id);
    if (record == nullptr || image == image_surfaces_.end()) {
        error = QStringLiteral("Select a pixel layer for the clone stamp.");
        return false;
    }
    double px = 0.0;
    double py = 0.0;
    if (!render::layer_pixel_at(record->transform, image->second.width(), image->second.height(), document_x,
                                document_y, px, py)) {
        error = QStringLiteral("The clone source is outside the layer.");
        return false;
    }
    clone_source_x_ = px;
    clone_source_y_ = py;
    clone_has_source_ = true;
    clone_layer_ = id;
    return true;
}

bool DocumentSession::clone_begin(const std::string& id, double document_x, double document_y, QString& error) {
    if (!clone_has_source_) {
        error = QStringLiteral("Alt-click to set a clone source first.");
        return false;
    }
    const model::ProjectLayerRecord* record = layer(id);
    ensure_surfaces();
    const auto image = image_surfaces_.find(id);
    if (record == nullptr || image == image_surfaces_.end()) {
        error = QStringLiteral("Select a pixel layer for the clone stamp.");
        return false;
    }
    double px = 0.0;
    double py = 0.0;
    if (!render::layer_pixel_at(record->transform, image->second.width(), image->second.height(), document_x,
                                document_y, px, py)) {
        return true;
    }
    clone_offset_x_ = px - clone_source_x_;
    clone_offset_y_ = py - clone_source_y_;
    push_history();
    cloning_ = true;
    clone_layer_ = id;
    clone_last_x_ = px;
    clone_last_y_ = py;
    render::clone_dab(image->second, px - clone_offset_x_, py - clone_offset_y_, px, py, brush_.radius, brush_.hardness,
                      brush_.opacity);
    return true;
}

void DocumentSession::clone_move(double document_x, double document_y) {
    if (!cloning_) {
        return;
    }
    const auto image = image_surfaces_.find(clone_layer_);
    const model::ProjectLayerRecord* record = layer(clone_layer_);
    if (image == image_surfaces_.end() || record == nullptr) {
        return;
    }
    double px = 0.0;
    double py = 0.0;
    if (!render::layer_pixel_at(record->transform, image->second.width(), image->second.height(), document_x,
                                document_y, px, py)) {
        return;
    }
    const double distance = std::hypot(px - clone_last_x_, py - clone_last_y_);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / std::max(1.0, brush_.radius * 0.25))));
    for (int i = 1; i <= steps; ++i) {
        const double t = static_cast<double>(i) / steps;
        const double dx = clone_last_x_ + (px - clone_last_x_) * t;
        const double dy = clone_last_y_ + (py - clone_last_y_) * t;
        render::clone_dab(image->second, dx - clone_offset_x_, dy - clone_offset_y_, dx, dy, brush_.radius,
                          brush_.hardness, brush_.opacity);
    }
    clone_last_x_ = px;
    clone_last_y_ = py;
}

void DocumentSession::clone_end() {
    if (!cloning_ || !snapshot_) {
        return;
    }
    cloning_ = false;
    const auto image = image_surfaces_.find(clone_layer_);
    if (image == image_surfaces_.end()) {
        return;
    }
    auto& asset = snapshot_->images[clone_layer_];
    asset.width = image->second.width();
    asset.height = image->second.height();
    asset.png = io::encode_png(image->second);
    QString error;
    recomposite(error);
}

bool DocumentSession::draw_gradient(const std::string& id, double a_red, double a_green, double a_blue, double a_alpha,
                                    double b_red, double b_green, double b_blue, double b_alpha, double start_x,
                                    double start_y, double end_x, double end_y, bool radial, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const model::ProjectLayerRecord* record = layer(id);
    if (record == nullptr || record->is_group_layer() || record->adjustment || !record->image_file) {
        error = QStringLiteral("Select a pixel layer for the gradient.");
        return false;
    }
    try {
        ensure_surfaces();
        render::RgbaSurface& surface = image_surfaces_.at(id);
        push_history();
        const render::Selection* clip = has_selection() ? &*selection_ : nullptr;
        render::draw_gradient(surface, record->transform, clip, a_red, a_green, a_blue, a_alpha, b_red, b_green, b_blue,
                              b_alpha, start_x, start_y, end_x, end_y, radial);
        auto& asset = snapshot_->images[id];
        asset.width = surface.width();
        asset.height = surface.height();
        asset.png = io::encode_png(surface);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

bool DocumentSession::fill_shape(const std::string& id, const render::Selection& shape, double red, double green,
                                 double blue, double opacity, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const model::ProjectLayerRecord* record = layer(id);
    if (record == nullptr || record->is_group_layer() || record->adjustment || !record->image_file) {
        error = QStringLiteral("Select a pixel layer to fill.");
        return false;
    }
    try {
        ensure_surfaces();
        render::RgbaSurface& surface = image_surfaces_.at(id);
        push_history();
        render::fill_in_layer(surface, record->transform, shape, red, green, blue, opacity);
        auto& asset = snapshot_->images[id];
        asset.width = surface.width();
        asset.height = surface.height();
        asset.png = io::encode_png(surface);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

bool DocumentSession::stroke_begin(const std::string& id, double document_x, double document_y, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const model::ProjectLayerRecord* record = layer(id);
    if (record == nullptr || record->is_group_layer() || record->adjustment || !record->image_file) {
        error = QStringLiteral("Select a pixel layer to paint on.");
        return false;
    }
    const auto found = snapshot_->images.find(id);
    if (found == snapshot_->images.end()) {
        error = QStringLiteral("This layer has no pixels to paint on.");
        return false;
    }
    if (image_surfaces_.count(id) == 0) {
        try {
            image_surfaces_.emplace(id, io::decode_image(found->second.png));
        } catch (const std::exception& failure) {
            error = QString::fromUtf8(failure.what());
            return false;
        }
    }
    render::RgbaSurface& surface = image_surfaces_.at(id);
    push_history();  // the pixels as they were, for undo
    stroke_layer_ = id;
    stroking_ = true;
    double px = 0.0;
    double py = 0.0;
    if (render::layer_pixel_at(record->transform, surface.width(), surface.height(), document_x, document_y, px, py)) {
        last_x_ = px;
        last_y_ = py;
        apply_brush(brush_, surface, px, py);
    }
    return recomposite(error);
}

bool DocumentSession::stroke_move(double document_x, double document_y, QString& error) {
    if (!stroking_) {
        return false;
    }
    const auto found = image_surfaces_.find(stroke_layer_);
    const model::ProjectLayerRecord* record = layer(stroke_layer_);
    if (found == image_surfaces_.end() || record == nullptr) {
        return false;
    }
    render::RgbaSurface& surface = found->second;
    double px = 0.0;
    double py = 0.0;
    if (!render::layer_pixel_at(record->transform, surface.width(), surface.height(), document_x, document_y, px, py)) {
        return false;
    }
    const double distance = std::hypot(px - last_x_, py - last_y_);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / std::max(1.0, brush_.radius * 0.25))));
    for (int i = 0; i <= steps; ++i) {
        const double t = static_cast<double>(i) / steps;
        apply_brush(brush_, surface, last_x_ + (px - last_x_) * t, last_y_ + (py - last_y_) * t);
    }
    last_x_ = px;
    last_y_ = py;
    return recomposite(error);
}

void DocumentSession::stroke_end() {
    if (!stroking_ || !snapshot_) {
        return;
    }
    stroking_ = false;
    const auto found = image_surfaces_.find(stroke_layer_);
    if (found == image_surfaces_.end()) {
        return;
    }
    try {
        auto& asset = snapshot_->images[stroke_layer_];
        asset.width = found->second.width();
        asset.height = found->second.height();
        asset.png = io::encode_png(found->second);
    } catch (const std::exception&) {
        // Keep the stroke on screen; saving will report the failure if it happens.
    }
}

}  // namespace compositor::appwin
