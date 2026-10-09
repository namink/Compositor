#include <algorithm>
#include <cmath>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "compositor/io/image_codec.hpp"
#include "compositor/render/placement.hpp"
#include "compositor/render/selection.hpp"
#include "document_session.hpp"

// Layer masks and clipping masks, split out so `document_session.cpp` stays within the size limit.

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

bool DocumentSession::add_layer_mask(const std::string& id, bool from_selection, bool invert, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    if (record == nullptr || record->is_group_layer() || record->adjustment) {
        error = QStringLiteral("Select a pixel layer for a mask.");
        return false;
    }
    try {
        ensure_surfaces();
        const auto image = image_surfaces_.find(id);
        if (image == image_surfaces_.end()) {
            error = QStringLiteral("This layer has no pixels for a mask.");
            return false;
        }
        const int width = image->second.width();
        const int height = image->second.height();
        const bool use_selection = from_selection && has_selection();
        render::RgbaSurface gray(width, height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int coverage = 255;
                if (use_selection) {
                    double doc_x = 0.0;
                    double doc_y = 0.0;
                    render::layer_pixel_to_document(record->transform, width, height, x + 0.5, y + 0.5, doc_x, doc_y);
                    coverage = selection_->at(static_cast<int>(std::floor(doc_x)), static_cast<int>(std::floor(doc_y)));
                }
                if (invert) {
                    coverage = 255 - coverage;
                }
                gray.set(x, y, static_cast<std::uint8_t>(coverage), static_cast<std::uint8_t>(coverage),
                         static_cast<std::uint8_t>(coverage), 255);
            }
        }
        push_history();
        model::ImageAsset asset;
        asset.width = width;
        asset.height = height;
        asset.png = io::encode_png(gray);
        snapshot_->masks[id] = std::move(asset);
        record->mask_file = id + ".mask.png";
        record->mask_enabled = true;
        invalidate_surfaces();
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

bool DocumentSession::remove_layer_mask(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    if (record == nullptr || !record->mask_file) {
        error = QStringLiteral("This layer has no mask.");
        return false;
    }
    push_history();
    record->mask_file.reset();
    record->mask_enabled.reset();
    record->mask_placement.reset();
    record->mask_linked.reset();
    snapshot_->masks.erase(id);
    invalidate_surfaces();
    return recomposite(error);
}

bool DocumentSession::invert_layer_mask(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    const auto mask = snapshot_->masks.find(id);
    if (record == nullptr || !record->mask_file || mask == snapshot_->masks.end()) {
        error = QStringLiteral("This layer has no mask to invert.");
        return false;
    }
    try {
        render::RgbaSurface surface = io::decode_image(mask->second.png);
        for (int y = 0; y < surface.height(); ++y) {
            for (int x = 0; x < surface.width(); ++x) {
                const std::uint8_t value = surface.data()[surface.offset(x, y)];
                surface.set(x, y, static_cast<std::uint8_t>(255 - value), static_cast<std::uint8_t>(255 - value),
                            static_cast<std::uint8_t>(255 - value), 255);
            }
        }
        push_history();
        mask->second.width = surface.width();
        mask->second.height = surface.height();
        mask->second.png = io::encode_png(surface);
        invalidate_surfaces();
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

bool DocumentSession::toggle_layer_mask(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    if (record == nullptr || !record->mask_file) {
        error = QStringLiteral("This layer has no mask.");
        return false;
    }
    push_history();
    record->mask_enabled = !record->mask_enabled.value_or(true);
    return recomposite(error);
}

bool DocumentSession::apply_layer_mask(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    model::ProjectLayerRecord* record = find(snapshot_->manifest, id);
    if (record == nullptr || !record->mask_file) {
        error = QStringLiteral("This layer has no mask.");
        return false;
    }
    try {
        ensure_surfaces();
        render::RgbaSurface& image = image_surfaces_.at(id);
        const auto mask = mask_surfaces_.find(id);
        if (mask != mask_surfaces_.end() && !mask->second.empty()) {
            const render::RgbaSurface& gray = mask->second;
            for (int y = 0; y < image.height(); ++y) {
                const int my = std::min(gray.height() - 1, y * gray.height() / image.height());
                for (int x = 0; x < image.width(); ++x) {
                    const int mx = std::min(gray.width() - 1, x * gray.width() / image.width());
                    const int coverage = gray.data()[gray.offset(mx, my)];
                    const std::size_t at = image.offset(x, y);
                    for (std::size_t c = 0; c < 4U; ++c) {
                        image.data()[at + c] = static_cast<std::uint8_t>(
                            (static_cast<unsigned>(image.data()[at + c]) * coverage + 127U) / 255U);
                    }
                }
            }
        }
        push_history();
        auto& asset = snapshot_->images[id];
        asset.width = image.width();
        asset.height = image.height();
        asset.png = io::encode_png(image);
        record->mask_file.reset();
        record->mask_enabled.reset();
        record->mask_placement.reset();
        record->mask_linked.reset();
        snapshot_->masks.erase(id);
        invalidate_surfaces();
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

bool DocumentSession::toggle_clipping_mask(const std::string& id, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    std::vector<model::ProjectLayerRecord>& layers = snapshot_->manifest.layers;
    const auto found = std::find_if(layers.begin(), layers.end(),
                                    [&id](const model::ProjectLayerRecord& record) { return record.id == id; });
    if (found == layers.end()) {
        error = QStringLiteral("Layer not found.");
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(found - layers.begin());
    push_history();
    if (layers[index].mask_source_id) {
        layers[index].mask_source_id.reset();
        return recomposite(error);
    }
    const std::optional<std::string>& parent = layers[index].parent_id;
    for (long other = static_cast<long>(index) - 1; other >= 0; --other) {
        if (layers[static_cast<std::size_t>(other)].parent_id != parent) {
            continue;
        }
        if (layers[static_cast<std::size_t>(other)].is_group_layer()) {
            break;
        }
        layers[index].mask_source_id = layers[static_cast<std::size_t>(other)].mask_source_id
                                           ? layers[static_cast<std::size_t>(other)].mask_source_id
                                           : std::optional<std::string>(layers[static_cast<std::size_t>(other)].id);
        break;
    }
    return recomposite(error);
}

void DocumentSession::adopt_clipping(const std::string& id) {
    if (!snapshot_) {
        return;
    }
    std::vector<model::ProjectLayerRecord>& layers = snapshot_->manifest.layers;
    auto self = std::find_if(layers.begin(), layers.end(),
                             [&id](const model::ProjectLayerRecord& record) { return record.id == id; });
    if (self == layers.end() || self->is_group_layer()) {
        return;
    }
    const std::optional<std::string> parent = self->parent_id;
    std::vector<model::ProjectLayerRecord*> siblings;
    for (model::ProjectLayerRecord& record : layers) {
        if (record.parent_id == parent) {
            siblings.push_back(&record);
        }
    }
    const auto position = std::find_if(siblings.begin(), siblings.end(),
                                       [&](model::ProjectLayerRecord* record) { return record->id == id; });
    if (position == siblings.begin() || position == siblings.end() || std::next(position) == siblings.end()) {
        return;
    }
    const std::optional<std::string> source = (*std::next(position))->mask_source_id;
    if (!source || *source == id) {
        return;
    }
    model::ProjectLayerRecord* below = *std::prev(position);
    if (below->id != *source && below->mask_source_id != source) {
        return;
    }
    (*position)->mask_source_id = source;
}

void DocumentSession::release_detached_clipping() {
    if (!snapshot_) {
        return;
    }
    std::vector<model::ProjectLayerRecord>& layers = snapshot_->manifest.layers;
    std::set<std::string> release;
    std::set<std::string> seen_parents;
    for (model::ProjectLayerRecord& record : layers) {
        const std::optional<std::string> parent = record.parent_id;
        const std::string key = parent ? *parent : std::string();
        if (!seen_parents.insert(key).second) {
            continue;
        }
        std::optional<std::string> base;
        for (model::ProjectLayerRecord& sibling : layers) {
            if (sibling.parent_id != parent) {
                continue;
            }
            if (sibling.mask_source_id) {
                if (sibling.mask_source_id != base) {
                    release.insert(sibling.id);
                    base = sibling.id;
                }
            } else {
                base = sibling.is_group_layer() ? std::nullopt : std::optional<std::string>(sibling.id);
            }
        }
    }
    for (model::ProjectLayerRecord& record : layers) {
        if (release.count(record.id) != 0) {
            record.mask_source_id.reset();
        }
    }
}

}  // namespace compositor::appwin
