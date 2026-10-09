#pragma once
#include <nlohmann/json.hpp>
#include <string>

#include "compositor/model/manifest.hpp"

namespace compositor::model {

void to_json(nlohmann::json& json, const LayerTransform& transform);
void from_json(const nlohmann::json& json, LayerTransform& transform);

void to_json(nlohmann::json& json, const CanvasGuide& guide);
void from_json(const nlohmann::json& json, CanvasGuide& guide);

void to_json(nlohmann::json& json, const ProjectLayerRecord& record);
void from_json(const nlohmann::json& json, ProjectLayerRecord& record);

void to_json(nlohmann::json& json, const ProjectManifest& manifest);
void from_json(const nlohmann::json& json, ProjectManifest& manifest);

/// Parse a manifest, throwing a `ProjectError` on malformed JSON or a bad field shape.
[[nodiscard]] ProjectManifest parse_manifest(const std::string& text);

/// Serialize with two-space indentation and sorted keys, matching the macOS app's writer.
[[nodiscard]] std::string serialize_manifest(const ProjectManifest& manifest);

/// The three fields read before the full manifest, so an unsupported version is refused cheaply.
struct ManifestHeader {
    std::string format;
    int version = 0;
};

[[nodiscard]] ManifestHeader parse_manifest_header(const std::string& text);

}  // namespace compositor::model
