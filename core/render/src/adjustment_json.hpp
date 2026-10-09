#pragma once
#include <cmath>
#include <nlohmann/json.hpp>
#include <string>

namespace compositor::render::detail {

/// Read a numeric field, falling back when it is missing or not finite.
[[nodiscard]] inline double number_or(const nlohmann::json& object, const char* key, double fallback) {
    if (!object.is_object() || !object.contains(key) || !object.at(key).is_number()) {
        return fallback;
    }
    const double value = object.at(key).get<double>();
    return std::isfinite(value) ? value : fallback;
}

[[nodiscard]] inline bool bool_or(const nlohmann::json& object, const char* key, bool fallback) {
    return object.is_object() && object.contains(key) && object.at(key).is_boolean() ? object.at(key).get<bool>()
                                                                                     : fallback;
}

[[nodiscard]] inline std::string string_or(const nlohmann::json& object, const char* key, const char* fallback) {
    return object.is_object() && object.contains(key) && object.at(key).is_string() ? object.at(key).get<std::string>()
                                                                                    : std::string(fallback);
}

/// A required child object, or an empty object when absent.
[[nodiscard]] inline const nlohmann::json& child(const nlohmann::json& object, const char* key) {
    static const nlohmann::json kEmpty = nlohmann::json::object();
    if (object.is_object() && object.contains(key) && object.at(key).is_object()) {
        return object.at(key);
    }
    return kEmpty;
}

}  // namespace compositor::render::detail
