#pragma once
#include <cstdint>
#include <vector>

#include "compositor/io/psd_reader.hpp"

namespace compositor::io {

/// Write an 8-bit RGB Photoshop file: one layer per `document.layers` raster layer (name, rectangle,
/// opacity, visibility and blend key), plus `composite` as the merged image data. Channel data is RAW
/// (uncompressed). Folders, layer masks and effects are flattened into the layers' pixels, so the
/// result round-trips through `read_psd` but keeps only what a flat PSD can hold.
[[nodiscard]] std::vector<std::uint8_t> write_psd(const PsdDocument& document, const render::RgbaSurface& composite);

}  // namespace compositor::io
