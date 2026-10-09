#include <cmath>
#include <exception>
#include <memory>
#include <string>

#include "compositor/io/image_codec.hpp"
#include "compositor/render/warp.hpp"
#include "document_session.hpp"

// Smudge and Liquify strokes, split out so `document_session_paint.cpp` stays within the soft size
// limit. Ported from the macOS app's `WarpStroke` driver in `SmudgeLiquify.swift`.

namespace compositor::appwin {

bool DocumentSession::warp_begin(const std::string& id, double x, double y, int mode, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const model::ProjectLayerRecord* record = layer(id);
    if (record == nullptr || record->is_group_layer() || record->adjustment) {
        error = QStringLiteral("Smudge and Liquify work on a pixel layer.");
        return false;
    }
    ensure_surfaces();
    const auto surface = image_surfaces_.find(id);
    if (surface == image_surfaces_.end()) {
        error = QStringLiteral("Smudge and Liquify work on a pixel layer.");
        return false;
    }
    push_history();
    warp_layer_ = id;
    warp_ = std::make_unique<render::WarpStroke>(surface->second,
                                                 mode == 1 ? render::WarpMode::smudge : render::WarpMode::liquify,
                                                 brush_.radius * 2.0, brush_.hardness, brush_.opacity);
    warp_->append(x, y);
    return true;
}

void DocumentSession::warp_move(double x, double y) {
    if (!warp_) {
        return;
    }
    warp_->append(x, y);
    QString error;
    recomposite(error);
}

void DocumentSession::warp_end() {
    if (!warp_) {
        return;
    }
    warp_.reset();
    const auto surface = image_surfaces_.find(warp_layer_);
    const auto image = snapshot_->images.find(warp_layer_);
    if (surface != image_surfaces_.end() && image != snapshot_->images.end()) {
        try {
            image->second.png = io::encode_png(surface->second);
        } catch (const std::exception&) {
            // Keep the result on screen; saving will report the failure if it happens.
        }
    }
    QString error;
    recomposite(error);
}

namespace {

/// The document point of the layer's unit point (u, v) in [0, 1], including rotation about the
/// center (flips are handled when sampling, as `DistortWarp.imageCorners` does).
void transform_point(const model::LayerTransform& transform, double u, double v, double& x, double& y) {
    const double center_x = transform.origin_x + transform.width / 2.0;
    const double center_y = transform.origin_y + transform.height / 2.0;
    const double dx = (u - 0.5) * transform.width;
    const double dy = (v - 0.5) * transform.height;
    const double radians = transform.rotation * 3.14159265358979323846 / 180.0;
    const double cos_a = std::cos(radians);
    const double sin_a = std::sin(radians);
    x = center_x + dx * cos_a - dy * sin_a;
    y = center_y + dx * sin_a + dy * cos_a;
}

}  // namespace

bool DocumentSession::layer_quad(const std::string& id, render::Quad& out) const {
    const model::ProjectLayerRecord* record = layer(id);
    if (record == nullptr || record->is_group_layer() || record->adjustment) {
        return false;
    }
    const double us[4] = {0.0, 1.0, 1.0, 0.0};
    const double vs[4] = {0.0, 0.0, 1.0, 1.0};
    for (int i = 0; i < 4; ++i) {
        transform_point(record->transform, us[i], vs[i], out.x[i], out.y[i]);
    }
    return true;
}

bool DocumentSession::apply_distort(const std::string& id, const render::Quad& corners, QString& error) {
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
    if (record == nullptr || record->is_group_layer() || record->adjustment) {
        error = QStringLiteral("Select a pixel layer to distort.");
        return false;
    }
    ensure_surfaces();
    const auto surface = image_surfaces_.find(id);
    if (surface == image_surfaces_.end()) {
        error = QStringLiteral("Select a pixel layer to distort.");
        return false;
    }
    int origin_x = 0;
    int origin_y = 0;
    render::RgbaSurface warped;
    if (!render::warp_perspective(surface->second, corners, record->transform.flip_x, record->transform.flip_y,
                                  origin_x, origin_y, warped)) {
        error = QStringLiteral("That distortion has no area.");
        return false;
    }
    push_history();
    record->transform.origin_x = origin_x;
    record->transform.origin_y = origin_y;
    record->transform.width = warped.width();
    record->transform.height = warped.height();
    record->transform.rotation = 0.0;
    record->transform.flip_x = false;
    record->transform.flip_y = false;
    try {
        model::ImageAsset& asset = snapshot_->images[id];
        asset.width = warped.width();
        asset.height = warped.height();
        asset.png = io::encode_png(warped);
        image_surfaces_[id] = std::move(warped);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

}  // namespace compositor::appwin
