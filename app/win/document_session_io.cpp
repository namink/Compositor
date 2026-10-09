#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "compositor/io/image_codec.hpp"
#include "compositor/io/psd_reader.hpp"
#include "compositor/model/limits.hpp"
#include "compositor/model/uuid.hpp"
#include "document_session.hpp"

// Image and Photoshop import, split out so `document_session.cpp` stays within the size limit.

namespace compositor::appwin {
namespace {

namespace fs = std::filesystem;

[[nodiscard]] std::vector<std::uint8_t> read_file(const QString& path) {
    std::ifstream input(fs::path(path.toStdWString()), std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open the image file.");
    }
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

[[nodiscard]] std::string stem_of(const QString& path) {
    const std::string name = fs::path(path.toStdWString()).filename().stem().string();
    return name.empty() ? std::string("Imported") : name;
}

/// The Compositor blend mode for a Photoshop blend key; unknown keys (Dissolve, Darker/Lighter Color)
/// fall back to Normal.
[[nodiscard]] model::LayerBlendMode blend_mode_from_psd(const std::string& key) {
    using model::LayerBlendMode;
    if (key == "mul ") {
        return LayerBlendMode::multiply;
    }
    if (key == "scrn") {
        return LayerBlendMode::screen;
    }
    if (key == "over") {
        return LayerBlendMode::overlay;
    }
    if (key == "sLit") {
        return LayerBlendMode::soft_light;
    }
    if (key == "dark") {
        return LayerBlendMode::darken;
    }
    if (key == "lite") {
        return LayerBlendMode::lighten;
    }
    if (key == "diff") {
        return LayerBlendMode::difference;
    }
    if (key == "div ") {
        return LayerBlendMode::color_dodge;
    }
    if (key == "idiv") {
        return LayerBlendMode::color_burn;
    }
    if (key == "hue ") {
        return LayerBlendMode::hue;
    }
    if (key == "sat ") {
        return LayerBlendMode::saturation;
    }
    if (key == "colr") {
        return LayerBlendMode::color;
    }
    if (key == "lum ") {
        return LayerBlendMode::luminosity;
    }
    if (key == "lbrn") {
        return LayerBlendMode::linear_burn;
    }
    if (key == "lddg") {
        return LayerBlendMode::linear_dodge;
    }
    if (key == "hLit") {
        return LayerBlendMode::hard_light;
    }
    if (key == "vLit") {
        return LayerBlendMode::vivid_light;
    }
    if (key == "lLit") {
        return LayerBlendMode::linear_light;
    }
    if (key == "pLit") {
        return LayerBlendMode::pin_light;
    }
    if (key == "hMix") {
        return LayerBlendMode::hard_mix;
    }
    if (key == "smud") {
        return LayerBlendMode::exclusion;
    }
    if (key == "fsub") {
        return LayerBlendMode::subtract;
    }
    if (key == "fdiv") {
        return LayerBlendMode::divide;
    }
    return LayerBlendMode::normal;
}

[[nodiscard]] model::ImageAsset mask_asset(const io::PsdLayer& layer) {
    render::RgbaSurface gray(layer.mask_width, layer.mask_height);
    for (int y = 0; y < layer.mask_height; ++y) {
        for (int x = 0; x < layer.mask_width; ++x) {
            const std::uint8_t coverage =
                layer.mask[static_cast<std::size_t>(y) * layer.mask_width + static_cast<std::size_t>(x)];
            gray.set(x, y, coverage, coverage, coverage, 255);
        }
    }
    model::ImageAsset asset;
    asset.width = layer.mask_width;
    asset.height = layer.mask_height;
    asset.png = io::encode_png(gray);
    return asset;
}

}  // namespace

bool DocumentSession::create(int width, int height, QString& error) {
    return create(width, height, 0U, error);
}

bool DocumentSession::create(int width, int height, std::uint32_t background_rgba, QString& error) {
    if (width < 1 || height < 1 || width > model::DocumentLimits::kMaxSide ||
        height > model::DocumentLimits::kMaxSide ||
        static_cast<std::int64_t>(width) * height > model::DocumentLimits::kMaxSurfacePixels) {
        error = QStringLiteral("That canvas size is out of range.");
        return false;
    }
    try {
        model::ProjectSnapshot snapshot;
        snapshot.manifest.document_id = model::generate_uuid();
        snapshot.manifest.width = width;
        snapshot.manifest.height = height;
        model::ProjectLayerRecord layer;
        layer.id = model::generate_uuid();
        layer.name = "Layer 1";
        layer.is_visible = true;
        layer.transform.width = width;
        layer.transform.height = height;
        layer.image_file = layer.id + ".png";
        render::RgbaSurface blank(width, height);
        if ((background_rgba & 0xFF000000U) != 0U) {
            const std::uint8_t r = static_cast<std::uint8_t>((background_rgba >> 24U) & 0xFFU);
            const std::uint8_t g = static_cast<std::uint8_t>((background_rgba >> 16U) & 0xFFU);
            const std::uint8_t b = static_cast<std::uint8_t>((background_rgba >> 8U) & 0xFFU);
            const std::uint8_t a = static_cast<std::uint8_t>(background_rgba & 0xFFU);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    blank.set(x, y, r, g, b, a);
                }
            }
        }
        model::ImageAsset asset;
        asset.width = width;
        asset.height = height;
        asset.png = io::encode_png(blank);
        snapshot.images[layer.id] = std::move(asset);
        snapshot.manifest.active_layer_id = layer.id;
        snapshot.manifest.layers.push_back(std::move(layer));
        snapshot_ = std::move(snapshot);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    undo_history_.clear();
    redo_history_.clear();
    undo_names_.clear();
    redo_names_.clear();
    pending_edit_name_ = "Edit";
    invalidate_surfaces();
    return recomposite(error);
}

bool DocumentSession::import_image(const QString& path, const std::string& active_id, QString& error) {
    render::RgbaSurface surface;
    try {
        const std::string extension = path.mid(path.lastIndexOf('.') + 1).toStdString();
        if (io::is_raw_extension(extension)) {
            surface = io::decode_raw_file(path.toStdString());
        } else {
            surface = io::decode_image(read_file(path));
        }
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    if (surface.empty()) {
        error = QStringLiteral("The image is empty.");
        return false;
    }

    model::ProjectLayerRecord record;
    record.id = model::generate_uuid();
    record.name = stem_of(path);
    record.is_visible = true;
    record.transform.origin_x = 0.0;
    record.transform.origin_y = 0.0;
    record.transform.width = surface.width();
    record.transform.height = surface.height();
    record.image_file = record.id + ".png";
    model::ImageAsset asset;
    asset.width = surface.width();
    asset.height = surface.height();
    try {
        asset.png = io::encode_png(surface);
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }

    if (!snapshot_) {
        // No document open: start one sized to the imported image.
        model::ProjectSnapshot snapshot;
        snapshot.manifest.document_id = model::generate_uuid();
        snapshot.manifest.width = surface.width();
        snapshot.manifest.height = surface.height();
        snapshot.manifest.active_layer_id = record.id;
        snapshot.manifest.layers.push_back(record);
        snapshot.images[record.id] = std::move(asset);
        snapshot_ = std::move(snapshot);
        undo_history_.clear();
        redo_history_.clear();
        undo_names_.clear();
        redo_names_.clear();
        invalidate_surfaces();
        return recomposite(error);
    }

    push_history();
    const std::string id = record.id;
    snapshot_->images[id] = std::move(asset);
    auto insert_at = snapshot_->manifest.layers.end();
    for (auto it = snapshot_->manifest.layers.begin(); it != snapshot_->manifest.layers.end(); ++it) {
        if (it->id == active_id) {
            insert_at = it + 1;
            break;
        }
    }
    snapshot_->manifest.layers.insert(insert_at, std::move(record));
    snapshot_->manifest.active_layer_id = id;
    return recomposite(error);
}

bool DocumentSession::import_psd(const QString& path, QString& error) {
    io::PsdDocument psd;
    try {
        psd = io::read_psd(read_file(path));
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    if (psd.width <= 0 || psd.height <= 0) {
        error = QStringLiteral("The Photoshop file has an empty canvas.");
        return false;
    }
    try {
        model::ProjectSnapshot snapshot;
        snapshot.manifest.document_id = model::generate_uuid();
        snapshot.manifest.width = psd.width;
        snapshot.manifest.height = psd.height;
        if (psd.resolution >= 1.0) {
            snapshot.manifest.resolution = psd.resolution;
        }
        std::vector<std::string> ids(psd.layers.size());
        for (std::string& id : ids) {
            id = model::generate_uuid();
        }
        for (std::size_t i = 0; i < psd.layers.size(); ++i) {
            const io::PsdLayer& source = psd.layers[i];
            model::ProjectLayerRecord record;
            record.id = ids[i];
            record.name = source.name.empty() ? "Layer" : source.name;
            record.is_visible = source.visible;
            record.opacity = source.opacity;
            if (source.parent >= 0 && static_cast<std::size_t>(source.parent) < ids.size()) {
                record.parent_id = ids[static_cast<std::size_t>(source.parent)];
            }
            if (source.is_group) {
                record.is_group = true;
                record.blend_mode = model::LayerBlendMode::normal;
                record.transform.origin_x = 0.0;
                record.transform.origin_y = 0.0;
                record.transform.width = psd.width;
                record.transform.height = psd.height;
            } else {
                record.blend_mode = blend_mode_from_psd(source.blend_key);
                record.transform.origin_x = source.left;
                record.transform.origin_y = source.top;
                record.transform.width = std::max(1.0, source.width);
                record.transform.height = std::max(1.0, source.height);
                if (!source.image.empty()) {
                    record.image_file = ids[i] + ".png";
                    model::ImageAsset asset;
                    asset.width = source.image.width();
                    asset.height = source.image.height();
                    asset.png = io::encode_png(source.image);
                    snapshot.images[ids[i]] = std::move(asset);
                }
            }
            if (source.has_mask && !source.mask.empty()) {
                record.mask_file = ids[i] + ".mask.png";
                record.mask_enabled = source.mask_enabled;
                if (!source.mask_linked) {
                    record.mask_linked = false;
                    model::LayerTransform placement;
                    placement.origin_x = source.mask_left;
                    placement.origin_y = source.mask_top;
                    placement.width = std::max(1.0, static_cast<double>(source.mask_width));
                    placement.height = std::max(1.0, static_cast<double>(source.mask_height));
                    record.mask_placement = placement;
                }
                snapshot.masks[ids[i]] = mask_asset(source);
            }
            snapshot.manifest.layers.push_back(std::move(record));
        }
        if (!snapshot.manifest.layers.empty()) {
            snapshot.manifest.active_layer_id = ids.back();
        }
        snapshot_ = std::move(snapshot);
        undo_history_.clear();
        redo_history_.clear();
        undo_names_.clear();
        redo_names_.clear();
        selection_.reset();
        invalidate_surfaces();
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return recomposite(error);
}

}  // namespace compositor::appwin
