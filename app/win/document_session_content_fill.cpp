#include <cmath>
#include <cstdint>
#include <exception>
#include <string>

#include "compositor/io/image_codec.hpp"
#include "compositor/render/paint.hpp"
#include "compositor/render/placement.hpp"
#include "document_session.hpp"

// Content-Aware Fill, ported from the macOS app's `ContentFill.swift`: the selected pixels of the
// active layer are synthesized from their unselected, opaque surroundings by the `content_fill` kernel.

namespace compositor::appwin {
namespace {

/// The selection in the layer's own pixel grid: for each layer pixel, the document coverage under it.
[[nodiscard]] render::Selection selection_in_layer(const render::Selection& selection,
                                                   const model::LayerTransform& transform, int width, int height) {
    render::Selection mapped;
    mapped.width = width;
    mapped.height = height;
    mapped.coverage.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
    mapped.min_x = 0;
    mapped.min_y = 0;
    mapped.max_x = width - 1;
    mapped.max_y = height - 1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double document_x = 0.0;
            double document_y = 0.0;
            render::layer_pixel_to_document(transform, width, height, static_cast<double>(x) + 0.5,
                                            static_cast<double>(y) + 0.5, document_x, document_y);
            const std::uint8_t coverage =
                selection.at(static_cast<int>(std::floor(document_x)), static_cast<int>(std::floor(document_y)));
            mapped
                .coverage[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
                coverage;
        }
    }
    return mapped;
}

}  // namespace

bool DocumentSession::content_aware_fill(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (!has_selection()) {
        error = QStringLiteral("Select an area to fill.");
        return false;
    }
    const model::ProjectLayerRecord* record = layer(id);
    if (record == nullptr || record->is_group_layer() || record->adjustment) {
        error = QStringLiteral("Select a pixel layer to fill.");
        return false;
    }
    ensure_surfaces();
    const auto cached = image_surfaces_.find(id);
    if (cached == image_surfaces_.end()) {
        error = QStringLiteral("Select a pixel layer to fill.");
        return false;
    }
    render::RgbaSurface working = cached->second;
    const render::Selection mapped =
        selection_in_layer(*selection_, record->transform, working.width(), working.height());
    try {
        if (!render::content_fill(working, mapped)) {
            error = QStringLiteral(
                "Not enough surrounding image to synthesize a fill. Use a smaller selection with some context.");
            return false;
        }
        push_history();
        cached->second = std::move(working);
        model::ImageAsset& asset = snapshot_->images[id];
        asset.width = cached->second.width();
        asset.height = cached->second.height();
        asset.png = io::encode_png(cached->second);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    selection_.reset();
    invalidate_surfaces();
    return recomposite(error);
}

}  // namespace compositor::appwin
