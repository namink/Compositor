#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <string>
#include <vector>

#include "compositor/io/image_codec.hpp"
#include "compositor/io/psd_writer.hpp"
#include "document_session.hpp"

// Photoshop export, split out so `document_session_io.cpp` stays within the soft size limit.

namespace compositor::appwin {
namespace {

/// The Photoshop blend key for a Compositor blend mode, the inverse of `blend_mode_from_psd`.
[[nodiscard]] std::string psd_key_from_blend(model::LayerBlendMode mode) {
    using model::LayerBlendMode;
    switch (mode) {
    case LayerBlendMode::multiply:
        return "mul ";
    case LayerBlendMode::screen:
        return "scrn";
    case LayerBlendMode::overlay:
        return "over";
    case LayerBlendMode::soft_light:
        return "sLit";
    case LayerBlendMode::darken:
        return "dark";
    case LayerBlendMode::lighten:
        return "lite";
    case LayerBlendMode::difference:
        return "diff";
    case LayerBlendMode::color_dodge:
        return "div ";
    case LayerBlendMode::color_burn:
        return "idiv";
    case LayerBlendMode::hue:
        return "hue ";
    case LayerBlendMode::saturation:
        return "sat ";
    case LayerBlendMode::color:
        return "colr";
    case LayerBlendMode::luminosity:
        return "lum ";
    case LayerBlendMode::linear_burn:
        return "lbrn";
    case LayerBlendMode::linear_dodge:
        return "lddg";
    case LayerBlendMode::hard_light:
        return "hLit";
    case LayerBlendMode::vivid_light:
        return "vLit";
    case LayerBlendMode::linear_light:
        return "lLit";
    case LayerBlendMode::pin_light:
        return "pLit";
    case LayerBlendMode::hard_mix:
        return "hMix";
    case LayerBlendMode::exclusion:
        return "smud";
    case LayerBlendMode::subtract:
        return "fsub";
    case LayerBlendMode::divide:
        return "fdiv";
    default:
        return "norm";
    }
}

}  // namespace

bool DocumentSession::export_psd(const QString& path, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    try {
        ensure_surfaces();
        io::PsdDocument document;
        document.width = snapshot_->manifest.width;
        document.height = snapshot_->manifest.height;
        for (const model::ProjectLayerRecord& record : snapshot_->manifest.layers) {
            if (record.is_group_layer() || record.adjustment || !record.image_file) {
                continue;
            }
            render::RgbaSurface surface;
            const auto cached = image_surfaces_.find(record.id);
            if (cached != image_surfaces_.end()) {
                surface = cached->second;
            } else {
                const auto image = snapshot_->images.find(record.id);
                if (image == snapshot_->images.end()) {
                    continue;
                }
                surface = io::decode_image(image->second.png);
            }
            io::PsdLayer layer;
            layer.name = record.name;
            layer.visible = record.is_visible;
            layer.opacity = record.effective_opacity();
            layer.blend_key = psd_key_from_blend(record.effective_blend_mode());
            layer.left = static_cast<double>(std::lround(record.transform.origin_x));
            layer.top = static_cast<double>(std::lround(record.transform.origin_y));
            layer.image = std::move(surface);
            document.layers.push_back(std::move(layer));
        }
        const std::vector<std::uint8_t> bytes = io::write_psd(document, flat_);
        std::ofstream out(path.toStdString(), std::ios::binary);
        if (!out) {
            error = QStringLiteral("Could not open the file for writing.");
            return false;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            error = QStringLiteral("Could not write the Photoshop file.");
            return false;
        }
    } catch (const std::exception& failure) {
        error = QString::fromUtf8(failure.what());
        return false;
    }
    return true;
}

}  // namespace compositor::appwin
