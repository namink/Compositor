#pragma once
#include <cstdint>
#include <filesystem>

namespace compositor::model::platform {

/// Total physical memory of the host, in bytes. Used to scale a document's raster budget the way the
/// macOS app scales it to the Mac's memory. Falls back to 8 GiB when the value cannot be read.
[[nodiscard]] std::uint64_t physical_memory_bytes();

/// Rename `source` over `destination`, replacing it. On Windows this is `MoveFileExW` with
/// `MOVEFILE_REPLACE_EXISTING` so the swap is as close to atomic as the platform allows. Throws
/// `ProjectError(encode)` on failure. `source` and `destination` must be on the same volume.
void atomic_replace_file(const std::filesystem::path& source, const std::filesystem::path& destination);

/// Replace the `destination` directory with `source`, both written under the same parent. A
/// directory cannot be atomically replaced on Windows, so this renames the old directory aside,
/// moves the new one in and deletes the old; on failure it restores the original. Throws
/// `ProjectError(encode)` on failure.
void atomic_replace_directory(const std::filesystem::path& source, const std::filesystem::path& destination);

}  // namespace compositor::model::platform
