#pragma once
#include <string>

namespace compositor::model {

/// True when `value` is a canonical RFC 4122 UUID in upper or lower case.
[[nodiscard]] bool is_valid_uuid(const std::string& value);

/// `value` in canonical upper case; a copy when it is already upper case.
///
/// Foundation writes UUIDs upper case (`6F1D...`), and the manifest's image-file names embed that
/// spelling, so the model normalizes to upper case everywhere. Assumes `value` is a valid UUID.
[[nodiscard]] std::string uppercase_uuid(std::string value);

/// A fresh random (version 4) UUID in canonical upper case.
[[nodiscard]] std::string generate_uuid();

}  // namespace compositor::model
