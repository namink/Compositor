#pragma once
#include "compositor/render/rgba_surface.hpp"

namespace compositor::render {

/// Gaussian blur of a premultiplied surface, `sigma` in pixels (the blur's standard deviation).
///
/// Outside the surface is transparent, matching the macOS app's Core Image path ("not clamped: the
/// blur softens the layer's edges rather than smearing the border outwards"). A true separable
/// convolution, so it is exact but O(w * h * sigma); the GPU path will take the full-size documents.
[[nodiscard]] RgbaSurface gaussian_blur(const RgbaSurface& source, double sigma);

/// Motion blur: a Gaussian smear along `angle_degrees` (Photoshop's counterclockwise convention,
/// measured to the same line either way) of length `distance` pixels. Photoshop smears evenly, Core
/// Image tapers like a Gaussian whose spread is `distance / sqrt(12)`; this follows the latter so the
/// streak density matches the macOS canvas.
[[nodiscard]] RgbaSurface motion_blur(const RgbaSurface& source, double distance, double angle_degrees);

}  // namespace compositor::render
