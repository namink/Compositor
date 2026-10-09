#pragma once
#include "compositor/model/layer_transform.hpp"
#include "compositor/render/rgba_surface.hpp"

namespace compositor::render {

/// Place a layer's pixels onto a document-sized surface, in document pixels.
///
/// `image` is the layer's decoded premultiplied pixels. `transform` is the layer's `LayerTransform`:
/// origin and size in document pixels, clockwise rotation about the center, and flips. Sampling is
/// bilinear unless `nearest`.
///
/// `mask`, when given, is the layer's decoded mask (coverage in the red channel, alpha ignored);
/// placed coverage multiplies the source alpha. When `mask_transform` is null the mask follows the
/// image (a linked mask, the common case); otherwise it is placed by its own transform on the
/// document (an unlinked mask). Coverage outside the mask's own rectangle reveals (1.0).
///
/// The result is the size of the document; areas the layer does not cover stay transparent.
[[nodiscard]] RgbaSurface place_layer(const RgbaSurface& image, const model::LayerTransform& transform, bool nearest,
                                      const RgbaSurface* mask, const model::LayerTransform* mask_transform,
                                      int document_width, int document_height);

/// A document-sized coverage map for a mask, where masked-in area is white and everything the mask
/// does not cover is black (0). Used to clip an adjustment layer's effect to its own mask, matching
/// the macOS app's `adjustmentClip`: the adjusted pixels show only where the mask reveals.
///
/// A null `mask_transform` links the mask to `transform` (its normalized extent maps onto the layer
/// rectangle); otherwise the mask is placed by its own transform. Coverage is read from the mask's
/// red channel.
[[nodiscard]] RgbaSurface place_mask_coverage(const RgbaSurface& mask, const model::LayerTransform& transform,
                                              const model::LayerTransform* mask_transform, int document_width,
                                              int document_height);

/// The layer's transform grown so an image padded by `inset` pixels on each side lands the same place
/// as the unpadded one, as the macOS app's `LayerEffectsRenderer.placed` does.
[[nodiscard]] model::LayerTransform grow_transform(const model::LayerTransform& transform, int image_width,
                                                   int image_height, double inset);

/// The layer's own pixel coordinates for a document point. False when the transform is degenerate.
[[nodiscard]] bool layer_pixel_at(const model::LayerTransform& transform, int image_width, int image_height,
                                  double document_x, double document_y, double& pixel_x, double& pixel_y);

/// Where a layer's pixel lands on the document. The inverse of `layer_pixel_at`.
void layer_pixel_to_document(const model::LayerTransform& transform, int image_width, int image_height, double pixel_x,
                             double pixel_y, double& document_x, double& document_y);

}  // namespace compositor::render