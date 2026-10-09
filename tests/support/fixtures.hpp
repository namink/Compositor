#pragma once
#include <string>
#include <vector>

#include "compositor/model/manifest.hpp"
#include "compositor/model/project_store.hpp"
#include "support/png_builder.hpp"

namespace comp_test {

// Fixed UUIDs so expected image-file names are stable across runs.
inline constexpr const char* kDocId = "0C5E7A91-3B2D-4F6A-8E1C-9D0B7A6F5E4D";
inline constexpr const char* kLayerA = "6F1D3C2A-0B7E-4E8A-9C4D-2A1B3C4D5E6F";
inline constexpr const char* kLayerB = "A1B2C3D4-E5F6-4A7B-8C9D-0E1F2A3B4C5D";
inline constexpr const char* kFolder = "11223344-5566-4778-899A-BBCCDDEEFF00";

using compositor::model::ImageAsset;
using compositor::model::LayerTransform;
using compositor::model::ProjectLayerRecord;
using compositor::model::ProjectManifest;
using compositor::model::ProjectSnapshot;

inline LayerTransform full_canvas_transform(int width, int height) {
    LayerTransform transform;
    transform.origin_x = 0;
    transform.origin_y = 0;
    transform.width = width;
    transform.height = height;
    return transform;
}

inline ProjectLayerRecord image_layer(const std::string& id, const std::string& name, int width, int height,
                                      bool visible = true) {
    ProjectLayerRecord record;
    record.id = id;
    record.name = name;
    record.is_visible = visible;
    record.transform = full_canvas_transform(width, height);
    record.image_file = id + ".png";
    return record;
}

/// A minimal, fully valid single-layer manifest at the given version (defaults to current).
inline ProjectManifest single_layer_manifest(int width = 4, int height = 4,
                                             int version = compositor::model::kCurrentFormatVersion) {
    ProjectManifest manifest;
    manifest.version = version;
    manifest.document_id = kDocId;
    manifest.width = width;
    manifest.height = height;
    manifest.layers.push_back(image_layer(kLayerA, "Background", width, height));
    manifest.active_layer_id = kLayerA;
    return manifest;
}

/// A snapshot carrying a valid PNG for every image and mask the manifest names.
inline ProjectSnapshot snapshot_with_images(const ProjectManifest& manifest, int color_type = 6) {
    ProjectSnapshot snapshot;
    snapshot.manifest = manifest;
    for (const ProjectLayerRecord& layer : manifest.layers) {
        if (layer.image_file) {
            ImageAsset asset;
            asset.width = static_cast<int>(layer.transform.width);
            asset.height = static_cast<int>(layer.transform.height);
            asset.png =
                make_png(static_cast<std::uint32_t>(asset.width), static_cast<std::uint32_t>(asset.height), color_type);
            snapshot.images[layer.id] = std::move(asset);
        }
        if (layer.mask_file) {
            ImageAsset mask;
            mask.width = static_cast<int>(layer.transform.width);
            mask.height = static_cast<int>(layer.transform.height);
            mask.png = make_png(static_cast<std::uint32_t>(mask.width), static_cast<std::uint32_t>(mask.height), 0);
            snapshot.masks[layer.id] = std::move(mask);
        }
    }
    return snapshot;
}

}  // namespace comp_test
