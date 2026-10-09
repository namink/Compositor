#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "compositor/render/rgba_surface.hpp"

namespace compositor::io {

/// A Photoshop file this reader cannot handle, or one that is damaged.
class PsdError : public std::runtime_error {
public:
    explicit PsdError(const std::string& message) : std::runtime_error(message) {}
};

/// One layer or folder read from a PSD, in file order (bottom to top, folders after their children).
struct PsdLayer {
    std::string name;
    bool is_group = false;
    bool visible = true;
    double opacity = 1.0;
    std::string blend_key = "norm";
    bool clipping = false;
    /// Index of the enclosing folder in `PsdDocument::layers`, or -1 at the root.
    int parent = -1;

    /// The layer rectangle on the canvas.
    double left = 0.0;
    double top = 0.0;
    double width = 0.0;
    double height = 0.0;

    /// Premultiplied sRGB pixels; empty for folders and blank layers.
    render::RgbaSurface image;

    bool has_mask = false;
    std::vector<std::uint8_t> mask;  // coverage, mask_width x mask_height
    int mask_width = 0;
    int mask_height = 0;
    double mask_left = 0.0;
    double mask_top = 0.0;
    std::uint8_t mask_default = 255;
    bool mask_enabled = true;
    bool mask_linked = true;
};

struct PsdDocument {
    int width = 0;
    int height = 0;
    double resolution = 72.0;
    std::vector<PsdLayer> layers;
};

/// True when `bytes` start with the 8BPS signature.
[[nodiscard]] bool looks_like_psd(std::span<const std::uint8_t> bytes);

/// Read an 8-bit RGB Photoshop file. Throws `PsdError` for unsupported versions, depths or color
/// modes (CMYK, 16/32-bit), and for damaged files.
[[nodiscard]] PsdDocument read_psd(std::span<const std::uint8_t> bytes);

}  // namespace compositor::io
