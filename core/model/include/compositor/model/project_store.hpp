#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "compositor/model/manifest.hpp"

namespace compositor::model {

/// A layer's or mask's encoded PNG together with its decoded pixel size.
struct ImageAsset {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> png;
};

/// A whole project in memory: the manifest plus each layer's pixels and mask, keyed by layer id.
/// Mirrors `ProjectSnapshot` in the macOS app.
struct ProjectSnapshot {
    ProjectManifest manifest;
    std::map<std::string, ImageAsset> images;
    std::map<std::string, ImageAsset> masks;
};

/// Read and write `.comp` packages. A `.comp` is an ordinary directory holding `manifest.json` and
/// an `images/` directory of `<layer id>.png` assets, so it works unchanged on any platform.
///
/// Saving is defensive: the new package is staged next to the destination and only swapped in once
/// it is complete, so a crash mid-save never leaves a half-written project.
class ProjectStore {
public:
    /// Read and validate a package. Throws `ProjectError` for anything the format forbids; an
    /// invalid package is rejected whole and never partially returned.
    [[nodiscard]] static ProjectSnapshot load(const std::filesystem::path& package);

    /// Write `snapshot` to `package`, replacing it. `quicklook_preview`, when given, is written as
    /// `QuickLook/Preview.jpg` for the platform's file preview. Throws `ProjectError` on failure.
    static void save(const ProjectSnapshot& snapshot, const std::filesystem::path& package,
                     const std::optional<std::vector<std::uint8_t>>& quicklook_preview = std::nullopt);
};

}  // namespace compositor::model
