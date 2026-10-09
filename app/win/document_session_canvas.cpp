#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "compositor/io/image_codec.hpp"
#include "compositor/model/uuid.hpp"
#include "compositor/render/placement.hpp"
#include "document_session.hpp"

namespace {

/// Resample a surface to a new pixel size, with a whole-surface transform (no rotation).
[[nodiscard]] compositor::render::RgbaSurface resample(const compositor::render::RgbaSurface& surface, int width,
                                                       int height, bool nearest) {
    compositor::model::LayerTransform transform;
    transform.origin_x = 0.0;
    transform.origin_y = 0.0;
    transform.width = width;
    transform.height = height;
    return compositor::render::place_layer(surface, transform, nearest, nullptr, nullptr, width, height);
}

}  // namespace

// Canvas Size, Image Size and Trim, split out so `document_session.cpp` stays within the size limit.

namespace compositor::appwin {

const std::vector<model::CanvasGuide>* DocumentSession::guides() const {
    if (!snapshot_ || !snapshot_->manifest.guides) {
        return nullptr;
    }
    return &*snapshot_->manifest.guides;
}

bool DocumentSession::add_guide(bool horizontal, double position, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    push_history();
    if (!snapshot_->manifest.guides) {
        snapshot_->manifest.guides = std::vector<model::CanvasGuide>{};
    }
    model::CanvasGuide guide;
    guide.id = model::generate_uuid();
    guide.axis = horizontal ? model::CanvasGuide::Axis::horizontal : model::CanvasGuide::Axis::vertical;
    guide.position = position;
    snapshot_->manifest.guides->push_back(std::move(guide));
    return true;
}

bool DocumentSession::clear_guides(QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    push_history();
    if (snapshot_->manifest.guides) {
        snapshot_->manifest.guides->clear();
    }
    return true;
}

bool DocumentSession::canvas_size(int width, int height, int anchor, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (width < 1 || height < 1) {
        error = QStringLiteral("The canvas needs a positive size.");
        return false;
    }
    const int column = std::clamp(anchor % 3, 0, 2);  // 0 left, 1 center, 2 right
    const int row = std::clamp(anchor / 3, 0, 2);     // 0 top, 1 middle, 2 bottom
    const double offset_x = (width - snapshot_->manifest.width) * (column / 2.0);
    const double offset_y = (height - snapshot_->manifest.height) * (row / 2.0);
    push_history();
    for (model::ProjectLayerRecord& layer : snapshot_->manifest.layers) {
        layer.transform.origin_x += offset_x;
        layer.transform.origin_y += offset_y;
        if (layer.mask_placement) {
            layer.mask_placement->origin_x += offset_x;
            layer.mask_placement->origin_y += offset_y;
        }
    }
    if (snapshot_->manifest.guides) {
        for (model::CanvasGuide& guide : *snapshot_->manifest.guides) {
            guide.position += guide.axis == model::CanvasGuide::Axis::vertical ? offset_x : offset_y;
        }
    }
    snapshot_->manifest.width = width;
    snapshot_->manifest.height = height;
    return recomposite(error);
}

bool DocumentSession::image_size(int width, int height, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (width < 1 || height < 1) {
        error = QStringLiteral("The image needs a positive size.");
        return false;
    }
    const double factor_x = static_cast<double>(width) / snapshot_->manifest.width;
    const double factor_y = static_cast<double>(height) / snapshot_->manifest.height;
    push_history();
    try {
        for (model::ProjectLayerRecord& layer : snapshot_->manifest.layers) {
            const bool nearest = layer.transform.sampling == model::LayerSampling::nearest;
            // Image Size resamples the pixels, as Photoshop does, keeping each layer's own proportions.
            auto image = snapshot_->images.find(layer.id);
            if (layer.image_file && image != snapshot_->images.end()) {
                const render::RgbaSurface surface = io::decode_image(image->second.png);
                const int new_w = std::max(1, static_cast<int>(std::lround(surface.width() * factor_x)));
                const int new_h = std::max(1, static_cast<int>(std::lround(surface.height() * factor_y)));
                const render::RgbaSurface scaled = resample(surface, new_w, new_h, nearest);
                image->second.width = new_w;
                image->second.height = new_h;
                image->second.png = io::encode_png(scaled);
            }
            auto mask = snapshot_->masks.find(layer.id);
            if (layer.mask_file && mask != snapshot_->masks.end()) {
                const render::RgbaSurface surface = io::decode_image(mask->second.png);
                const int new_w = std::max(1, static_cast<int>(std::lround(surface.width() * factor_x)));
                const int new_h = std::max(1, static_cast<int>(std::lround(surface.height() * factor_y)));
                const render::RgbaSurface scaled = resample(surface, new_w, new_h, nearest);
                mask->second.width = new_w;
                mask->second.height = new_h;
                mask->second.png = io::encode_png(scaled);
            }
            layer.transform.origin_x *= factor_x;
            layer.transform.origin_y *= factor_y;
            layer.transform.width *= factor_x;
            layer.transform.height *= factor_y;
            if (layer.mask_placement) {
                layer.mask_placement->origin_x *= factor_x;
                layer.mask_placement->origin_y *= factor_y;
                layer.mask_placement->width *= factor_x;
                layer.mask_placement->height *= factor_y;
            }
        }
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    if (snapshot_->manifest.guides) {
        for (model::CanvasGuide& guide : *snapshot_->manifest.guides) {
            guide.position *= guide.axis == model::CanvasGuide::Axis::vertical ? factor_x : factor_y;
        }
    }
    snapshot_->manifest.width = width;
    snapshot_->manifest.height = height;
    invalidate_surfaces();
    return recomposite(error);
}

bool DocumentSession::trim(QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (flat_.empty()) {
        error = QStringLiteral("Nothing to trim.");
        return false;
    }
    int min_x = flat_.width();
    int min_y = flat_.height();
    int max_x = -1;
    int max_y = -1;
    for (int y = 0; y < flat_.height(); ++y) {
        for (int x = 0; x < flat_.width(); ++x) {
            if (flat_.data()[flat_.offset(x, y) + 3] != 0) {
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        }
    }
    if (max_x < min_x || max_y < min_y) {
        error = QStringLiteral("The image is empty.");
        return false;
    }
    push_history();
    for (model::ProjectLayerRecord& layer : snapshot_->manifest.layers) {
        layer.transform.origin_x -= min_x;
        layer.transform.origin_y -= min_y;
        if (layer.mask_placement) {
            layer.mask_placement->origin_x -= min_x;
            layer.mask_placement->origin_y -= min_y;
        }
    }
    if (snapshot_->manifest.guides) {
        for (model::CanvasGuide& guide : *snapshot_->manifest.guides) {
            guide.position -= guide.axis == model::CanvasGuide::Axis::vertical ? min_x : min_y;
        }
    }
    snapshot_->manifest.width = max_x - min_x + 1;
    snapshot_->manifest.height = max_y - min_y + 1;
    return recomposite(error);
}

}  // namespace compositor::appwin
