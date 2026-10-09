#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "compositor/io/image_codec.hpp"
#include "compositor/model/uuid.hpp"
#include "compositor/render/compositor.hpp"
#include "document_session.hpp"

// Merge Down / Merge Group, ported from the macOS app's `LayerMerge.swift`.

namespace compositor::appwin {
namespace {

struct MergePlan {
    std::vector<std::string> ids;
    std::set<std::string> removed;
    std::string name;
    std::optional<std::string> parent;
    std::string anchor;
};

[[nodiscard]] const model::ProjectLayerRecord* find(const model::ProjectManifest& manifest, const std::string& id) {
    for (const model::ProjectLayerRecord& record : manifest.layers) {
        if (record.id == id) {
            return &record;
        }
    }
    return nullptr;
}

/// Everything inside a folder, folders included.
[[nodiscard]] std::set<std::string> descendants(const model::ProjectManifest& manifest, const std::string& id) {
    std::set<std::string> result;
    std::vector<std::string> pending{id};
    while (!pending.empty()) {
        const std::string parent = pending.back();
        pending.pop_back();
        for (const model::ProjectLayerRecord& record : manifest.layers) {
            if (record.parent_id && *record.parent_id == parent && result.insert(record.id).second) {
                pending.push_back(record.id);
            }
        }
    }
    return result;
}

/// The plan for the active layer: a folder merges its contents, otherwise it merges down.
[[nodiscard]] std::optional<MergePlan> plan_for(const model::ProjectManifest& manifest, const std::string& active_id) {
    const model::ProjectLayerRecord* active = find(manifest, active_id);
    if (active == nullptr) {
        return std::nullopt;
    }
    MergePlan plan;
    if (active->is_group_layer()) {
        const std::set<std::string> inside = descendants(manifest, active_id);
        const bool has_pixels =
            std::any_of(manifest.layers.begin(), manifest.layers.end(), [&](const model::ProjectLayerRecord& record) {
                return inside.count(record.id) != 0 && !record.is_group_layer();
            });
        if (!has_pixels) {
            return std::nullopt;
        }
        for (const model::ProjectLayerRecord& record : manifest.layers) {
            if (inside.count(record.id) != 0 || record.id == active_id) {
                plan.ids.push_back(record.id);
            }
        }
        plan.removed.insert(plan.ids.begin(), plan.ids.end());
        plan.name = active->name;
        plan.parent = active->parent_id;
        plan.anchor = active_id;
        return plan;
    }
    const auto index = std::find_if(manifest.layers.begin(), manifest.layers.end(),
                                    [&](const model::ProjectLayerRecord& record) { return record.id == active_id; });
    const std::size_t position = static_cast<std::size_t>(index - manifest.layers.begin());
    const model::ProjectLayerRecord* below = nullptr;
    for (std::size_t i = position; i-- > 0;) {
        if (manifest.layers[i].parent_id == active->parent_id) {
            if (manifest.layers[i].is_group_layer()) {
                return std::nullopt;
            }
            below = &manifest.layers[i];
            break;
        }
    }
    if (below == nullptr) {
        return std::nullopt;
    }
    plan.ids = {below->id, active_id};
    plan.removed = {below->id, active_id};
    plan.name = below->name;
    plan.parent = active->parent_id;
    plan.anchor = active_id;
    return plan;
}

/// The non-transparent bounding box of a surface, or null when it is empty.
[[nodiscard]] std::optional<model::LayerTransform> visible_bounds(const render::RgbaSurface& surface) {
    int min_x = surface.width();
    int min_y = surface.height();
    int max_x = -1;
    int max_y = -1;
    for (int y = 0; y < surface.height(); ++y) {
        for (int x = 0; x < surface.width(); ++x) {
            if (surface.data()[surface.offset(x, y) + 3] != 0) {
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        }
    }
    if (max_x < min_x || max_y < min_y) {
        return std::nullopt;
    }
    model::LayerTransform transform;
    transform.origin_x = min_x;
    transform.origin_y = min_y;
    transform.width = max_x - min_x + 1;
    transform.height = max_y - min_y + 1;
    return transform;
}

[[nodiscard]] render::RgbaSurface crop(const render::RgbaSurface& source, const model::LayerTransform& box) {
    const int x0 = static_cast<int>(box.origin_x);
    const int y0 = static_cast<int>(box.origin_y);
    const int width = static_cast<int>(box.width);
    const int height = static_cast<int>(box.height);
    render::RgbaSurface result(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t at = source.offset(x0 + x, y0 + y);
            const std::uint8_t* pixel = source.data() + at;
            result.set(x, y, pixel[0], pixel[1], pixel[2], pixel[3]);
        }
    }
    return result;
}

}  // namespace

bool DocumentSession::merge_layers(QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    const std::optional<std::string> active = snapshot_->manifest.active_layer_id;
    if (!active) {
        error = QStringLiteral("Select a layer to merge.");
        return false;
    }
    const std::optional<MergePlan> plan = plan_for(snapshot_->manifest, *active);
    if (!plan) {
        error = QStringLiteral("There is nothing to merge with this layer.");
        return false;
    }

    // Composite only the layers being merged, cut loose from anything outside the merge.
    model::ProjectManifest subset;
    subset.width = snapshot_->manifest.width;
    subset.height = snapshot_->manifest.height;
    for (const model::ProjectLayerRecord& record : snapshot_->manifest.layers) {
        if (std::find(plan->ids.begin(), plan->ids.end(), record.id) == plan->ids.end()) {
            continue;
        }
        model::ProjectLayerRecord copy = record;
        if (copy.parent_id && plan->removed.count(*copy.parent_id) == 0) {
            copy.parent_id.reset();
        }
        if (copy.mask_source_id && plan->removed.count(*copy.mask_source_id) == 0) {
            copy.mask_source_id.reset();
        }
        subset.layers.push_back(std::move(copy));
    }

    try {
        std::map<std::string, render::RgbaSurface> subset_images;
        std::map<std::string, render::RgbaSurface> subset_masks;
        for (const std::string& id : plan->ids) {
            const auto image = snapshot_->images.find(id);
            if (image != snapshot_->images.end()) {
                subset_images.emplace(id, io::decode_image(image->second.png));
            }
            const auto mask = snapshot_->masks.find(id);
            if (mask != snapshot_->masks.end()) {
                subset_masks.emplace(id, io::decode_image(mask->second.png));
            }
        }
        const render::RgbaSurface flat = render::composite_document(subset, subset_images, subset_masks);
        const std::optional<model::LayerTransform> box = visible_bounds(flat);
        if (!box) {
            error = QStringLiteral("The merged layers are empty.");
            return false;
        }
        const render::RgbaSurface trimmed = crop(flat, *box);

        push_history();
        model::ProjectLayerRecord merged;
        merged.id = model::generate_uuid();
        merged.name = plan->name;
        merged.is_visible = true;
        merged.transform = *box;
        merged.image_file = merged.id + ".png";
        model::ImageAsset asset;
        asset.width = trimmed.width();
        asset.height = trimmed.height();
        asset.png = io::encode_png(trimmed);
        const std::string merged_id = merged.id;
        snapshot_->images[merged_id] = std::move(asset);

        std::vector<model::ProjectLayerRecord> next;
        next.reserve(snapshot_->manifest.layers.size());
        for (const model::ProjectLayerRecord& record : snapshot_->manifest.layers) {
            if (plan->removed.count(record.id) == 0) {
                next.push_back(record);
            }
        }
        // Layers clipped to something that was merged now clip to the result.
        for (model::ProjectLayerRecord& record : next) {
            if (record.mask_source_id && plan->removed.count(*record.mask_source_id) != 0) {
                record.mask_source_id = merged_id;
            }
        }
        const auto anchor =
            std::find_if(snapshot_->manifest.layers.begin(), snapshot_->manifest.layers.end(),
                         [&](const model::ProjectLayerRecord& record) { return record.id == plan->anchor; });
        const std::size_t slot = static_cast<std::size_t>(anchor - snapshot_->manifest.layers.begin());
        std::size_t insertion = slot;
        for (std::size_t i = 0; i < slot; ++i) {
            if (plan->removed.count(snapshot_->manifest.layers[i].id) != 0) {
                --insertion;
            }
        }
        insertion = std::min(insertion, next.size());
        next.insert(next.begin() + static_cast<std::ptrdiff_t>(insertion), std::move(merged));
        snapshot_->manifest.layers = std::move(next);
        snapshot_->manifest.active_layer_id = merged_id;
        for (const std::string& removed_id : plan->removed) {
            snapshot_->images.erase(removed_id);
            snapshot_->masks.erase(removed_id);
        }
        invalidate_surfaces();
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

}  // namespace compositor::appwin
