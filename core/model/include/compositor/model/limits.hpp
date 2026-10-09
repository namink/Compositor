#pragma once
#include <cstddef>
#include <cstdint>

namespace compositor::model {

/// The size and memory ceilings a document is held to, in one place.
///
/// These mirror `DocumentLimits.swift` in the macOS app. Two ideas are kept separate on purpose:
/// how large a *single* surface may be, and how much raster a *whole* document may hold across all
/// of its layers. A 58-megapixel print banner with 29 layers is ordinary and needs far more than
/// one surface's worth of allowance even though no single surface in it is unusual.
struct DocumentLimits {
    /// Longest side, in pixels, of any canvas, layer, mask or generated surface.
    static constexpr int kMaxSide = 30000;
    /// Largest single surface: a canvas, an export, a filter target or a mask render.
    static constexpr std::int64_t kMaxSurfacePixels = 200'000'000;
    /// Most layers one manifest may list.
    static constexpr std::size_t kMaxLayers = 10000;
    /// Manifest size ceiling.
    static constexpr std::int64_t kMaxManifestBytes = 4LL * 1024 * 1024;
    /// Per encoded asset (a layer or mask PNG) ceiling.
    static constexpr std::int64_t kMaxAssetBytes = 512LL * 1024 * 1024;
    /// Deepest a layer may nest inside folders.
    static constexpr int kMaxAncestorLevels = 64;
    /// Longest a live-mask (clipping) chain may run.
    static constexpr int kMaxLiveMaskChain = 256;
    /// Longest a layer name may be, in UTF-8 bytes.
    static constexpr std::size_t kMaxNameBytes = 16384;
    /// Most alignment guides a document may store.
    static constexpr int kMaxGuides = 1000;
    /// Largest absolute guide position, in document pixels.
    static constexpr double kMaxGuidePosition = 1'000'000.0;

    /// Total imported raster one document may hold. Scaled to the machine: a quarter of its memory
    /// at 4 bytes a pixel, never below one surface and never above 800 MP. The macOS app uses the
    /// same formula against `ProcessInfo.physicalMemory`; here it is the host's physical memory.
    static std::int64_t document_pixel_budget();
};

}  // namespace compositor::model
