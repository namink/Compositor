#pragma once
#include <stdexcept>
#include <string>

namespace compositor::model {

/// The format identifier stored in every manifest.
inline constexpr const char* kFormatIdentifier = "com.compositor.project";
/// The color space every document works in. Only sRGB is accepted.
inline constexpr const char* kColorSpace = "sRGB";
/// The format version new saves write.
inline constexpr int kCurrentFormatVersion = 11;
/// The oldest version that can still be read.
inline constexpr int kMinSupportedFormatVersion = 1;

enum class ProjectErrorCode {
    /// Not a Compositor project, or its metadata is damaged.
    invalid,
    /// A format version this build does not support.
    unsupported_version,
    /// An image named by the manifest is missing or damaged.
    missing_image,
    /// Beyond the canvas, layer, file-size or document budget.
    too_large,
    /// An asset could not be written.
    encode,
};

/// Thrown by load/save and by validation. Carries the format version for `unsupported_version`.
class ProjectError : public std::runtime_error {
public:
    ProjectError(ProjectErrorCode code, std::string message, int version = 0);

    [[nodiscard]] ProjectErrorCode code() const noexcept { return code_; }
    [[nodiscard]] int version() const noexcept { return version_; }

private:
    ProjectErrorCode code_;
    int version_;
};

/// The reader-facing text for a code, matching the macOS app's wording.
[[nodiscard]] std::string project_error_message(ProjectErrorCode code, int version);

}  // namespace compositor::model
