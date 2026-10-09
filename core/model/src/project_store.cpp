#include "compositor/model/project_store.hpp"

#include <cstddef>
#include <fstream>
#include <system_error>
#include <utility>

#include "compositor/model/errors.hpp"
#include "compositor/model/limits.hpp"
#include "compositor/model/manifest_json.hpp"
#include "compositor/model/png_header.hpp"
#include "compositor/model/uuid.hpp"
#include "compositor/model/validation.hpp"
#include "platform/platform.hpp"

namespace fs = std::filesystem;

namespace compositor::model {
namespace {

[[noreturn]] void too_large() {
    throw ProjectError(ProjectErrorCode::too_large, project_error_message(ProjectErrorCode::too_large, 0));
}

[[noreturn]] void missing_image() {
    throw ProjectError(ProjectErrorCode::missing_image, project_error_message(ProjectErrorCode::missing_image, 0));
}

/// Read a whole file, refusing anything the format does not allow before reading it. `missing` names
/// the error for an absent or unreadable file: `invalid` for the manifest, `missing_image` for an
/// asset, so callers can tell a damaged package from a missing layer image.
std::vector<std::uint8_t> read_file(const fs::path& path, std::int64_t maximum_bytes, ProjectErrorCode missing) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        throw ProjectError(missing, project_error_message(missing, 0));
    }
    const auto size = fs::file_size(path, ec);
    if (ec || static_cast<std::int64_t>(size) > maximum_bytes) {
        too_large();
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    std::ifstream input(path, std::ios::binary);
    if (!input || (size > 0 && !input.read(reinterpret_cast<char*>(bytes.data()), size))) {
        throw ProjectError(missing, project_error_message(missing, 0));
    }
    return bytes;
}

void write_file(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output || (!bytes.empty() && !output.write(reinterpret_cast<const char*>(bytes.data()),
                                                    static_cast<std::streamsize>(bytes.size())))) {
        throw ProjectError(ProjectErrorCode::encode, "An image or manifest could not be written.");
    }
}

/// Charge `width * height` against one of the two independent pixel budgets, as the macOS app does:
/// layer pixels and mask pixels each get a full document budget rather than sharing one.
void charge_pixels(int width, int height, std::int64_t& used) {
    const bool sides_ok =
        width >= 1 && width <= DocumentLimits::kMaxSide && height >= 1 && height <= DocumentLimits::kMaxSide;
    const std::int64_t pixels = static_cast<std::int64_t>(width) * static_cast<std::int64_t>(height);
    if (!sides_ok || pixels > DocumentLimits::document_pixel_budget() - used) {
        too_large();
    }
    used += pixels;
}

const ImageAsset& require_asset(const ProjectSnapshot& snapshot, const ProjectLayerRecord& layer, bool is_mask) {
    const auto& table = is_mask ? snapshot.masks : snapshot.images;
    const auto found = table.find(layer.id);
    if (found == table.end()) {
        missing_image();
    }
    return found->second;
}

std::string short_suffix() {
    const std::string uuid = generate_uuid();
    return uuid.substr(0, 8);
}

/// Remove the staging directory unless the save committed.
struct StagingGuard {
    fs::path path;
    bool committed = false;
    ~StagingGuard() {
        if (!committed) {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    }
};

}  // namespace

ProjectSnapshot ProjectStore::load(const fs::path& package) {
    std::error_code ec;
    if (!fs::is_directory(package, ec)) {
        throw ProjectError(ProjectErrorCode::invalid, "The project package is not a directory.");
    }
    const std::vector<std::uint8_t> metadata =
        read_file(package / "manifest.json", DocumentLimits::kMaxManifestBytes, ProjectErrorCode::invalid);
    const std::string text(metadata.begin(), metadata.end());
    const ManifestHeader header = parse_manifest_header(text);
    if (header.format != kFormatIdentifier) {
        throw ProjectError(ProjectErrorCode::invalid, "This is not a Compositor project.");
    }
    if (header.version < kMinSupportedFormatVersion || header.version > kCurrentFormatVersion) {
        throw ProjectError(ProjectErrorCode::unsupported_version,
                           project_error_message(ProjectErrorCode::unsupported_version, header.version),
                           header.version);
    }

    ProjectSnapshot snapshot;
    snapshot.manifest = parse_manifest(text);
    validate_manifest(snapshot.manifest);

    std::int64_t image_pixels = 0;
    std::int64_t mask_pixels = 0;
    for (const ProjectLayerRecord& layer : snapshot.manifest.layers) {
        for (const bool is_mask : {false, true}) {
            const std::optional<std::string>& filename = is_mask ? layer.mask_file : layer.image_file;
            if (!filename) {
                continue;
            }
            const std::vector<std::uint8_t> bytes = read_file(
                package / "images" / *filename, DocumentLimits::kMaxAssetBytes, ProjectErrorCode::missing_image);
            const PngInfo info = parse_png_header(bytes);
            if (!info.is_png || info.bit_depth > 8) {
                missing_image();
            }
            charge_pixels(info.width, info.height, is_mask ? mask_pixels : image_pixels);
            ImageAsset asset;
            asset.width = info.width;
            asset.height = info.height;
            asset.png = bytes;
            (is_mask ? snapshot.masks : snapshot.images)[layer.id] = std::move(asset);
        }
    }
    return snapshot;
}

void ProjectStore::save(const ProjectSnapshot& snapshot, const fs::path& package,
                        const std::optional<std::vector<std::uint8_t>>& quicklook_preview) {
    validate_manifest(snapshot.manifest);

    const fs::path parent = package.has_parent_path() ? package.parent_path() : fs::path(".");
    StagingGuard staging{parent / (package.filename().string() + ".tmp-" + short_suffix())};
    std::error_code ec;
    fs::remove_all(staging.path, ec);
    if (!fs::create_directories(staging.path / "images", ec)) {
        throw ProjectError(ProjectErrorCode::encode, "The project could not be staged for saving.");
    }

    std::int64_t image_pixels = 0;
    std::int64_t mask_pixels = 0;
    for (const ProjectLayerRecord& layer : snapshot.manifest.layers) {
        for (const bool is_mask : {false, true}) {
            const std::optional<std::string>& filename = is_mask ? layer.mask_file : layer.image_file;
            if (!filename) {
                continue;
            }
            const ImageAsset& asset = require_asset(snapshot, layer, is_mask);
            charge_pixels(asset.width, asset.height, is_mask ? mask_pixels : image_pixels);
            write_file(staging.path / "images" / *filename, asset.png);
        }
    }

    const std::string metadata = serialize_manifest(snapshot.manifest);
    if (static_cast<std::int64_t>(metadata.size()) > DocumentLimits::kMaxManifestBytes) {
        too_large();
    }
    write_file(staging.path / "manifest.json", std::vector<std::uint8_t>(metadata.begin(), metadata.end()));

    if (quicklook_preview) {
        if (!fs::create_directories(staging.path / "QuickLook", ec)) {
            throw ProjectError(ProjectErrorCode::encode, "The project preview could not be staged.");
        }
        write_file(staging.path / "QuickLook" / "Preview.jpg", *quicklook_preview);
    }

    platform::atomic_replace_directory(staging.path, package);
    staging.committed = true;
}

}  // namespace compositor::model
