#include "compositor/model/errors.hpp"

#include "compositor/model/limits.hpp"

namespace compositor::model {

ProjectError::ProjectError(ProjectErrorCode code, std::string message, int version)
    : std::runtime_error(std::move(message)), code_(code), version_(version) {}

std::string project_error_message(ProjectErrorCode code, int version) {
    switch (code) {
    case ProjectErrorCode::invalid:
        return "This is not a valid Compositor project, or its metadata is damaged.";
    case ProjectErrorCode::unsupported_version:
        return "This project uses format version " + std::to_string(version) + ". This app supports versions " +
               std::to_string(kMinSupportedFormatVersion) + "\u2013" + std::to_string(kCurrentFormatVersion) + ".";
    case ProjectErrorCode::missing_image:
        return "An image inside the project is missing or damaged. The current document has not been replaced.";
    case ProjectErrorCode::too_large:
        return "This project exceeds the supported canvas, layer, file-size, or " +
               std::to_string(DocumentLimits::document_pixel_budget() / 1'000'000) + "-megapixel document limit.";
    case ProjectErrorCode::encode:
        return "An image could not be saved. The previous project has not been replaced.";
    }
    return "Unknown project error.";
}

}  // namespace compositor::model
