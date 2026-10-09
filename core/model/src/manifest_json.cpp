#include "compositor/model/manifest_json.hpp"

#include <string>

#include "compositor/model/errors.hpp"
#include "compositor/model/uuid.hpp"

namespace compositor::model {
namespace {

using nlohmann::json;

[[noreturn]] void bad_field(const std::string& field) {
    throw ProjectError(ProjectErrorCode::invalid, "Manifest field '" + field + "' is missing or malformed.");
}

const json& required(const json& object, const char* key) {
    if (!object.is_object() || !object.contains(key) || object.at(key).is_null()) {
        bad_field(key);
    }
    return object.at(key);
}

std::string required_uuid(const json& object, const char* key) {
    const json& value = required(object, key);
    if (!value.is_string() || !is_valid_uuid(value.get<std::string>())) {
        bad_field(key);
    }
    return uppercase_uuid(value.get<std::string>());
}

std::string required_string(const json& object, const char* key) {
    const json& value = required(object, key);
    if (!value.is_string()) {
        bad_field(key);
    }
    return value.get<std::string>();
}

bool required_bool(const json& object, const char* key) {
    const json& value = required(object, key);
    if (!value.is_boolean()) {
        bad_field(key);
    }
    return value.get<bool>();
}

int required_int(const json& object, const char* key) {
    const json& value = required(object, key);
    if (!value.is_number_integer()) {
        bad_field(key);
    }
    return value.get<int>();
}

double required_number(const json& object, const char* key) {
    const json& value = required(object, key);
    if (!value.is_number()) {
        bad_field(key);
    }
    return value.get<double>();
}

void read_pair(const json& object, const char* key, double& first, double& second) {
    const json& value = required(object, key);
    if (!value.is_array() || value.size() != 2 || !value.at(0).is_number() || !value.at(1).is_number()) {
        bad_field(key);
    }
    first = value.at(0).get<double>();
    second = value.at(1).get<double>();
}

void read_optional_uuid(const json& object, const char* key, std::optional<std::string>& out) {
    if (!object.contains(key) || object.at(key).is_null()) {
        return;
    }
    const json& value = object.at(key);
    if (!value.is_string() || !is_valid_uuid(value.get<std::string>())) {
        bad_field(key);
    }
    out = uppercase_uuid(value.get<std::string>());
}

void read_optional_string(const json& object, const char* key, std::optional<std::string>& out) {
    if (!object.contains(key) || object.at(key).is_null()) {
        return;
    }
    if (!object.at(key).is_string()) {
        bad_field(key);
    }
    out = object.at(key).get<std::string>();
}

void read_optional_bool(const json& object, const char* key, std::optional<bool>& out) {
    if (!object.contains(key) || object.at(key).is_null()) {
        return;
    }
    if (!object.at(key).is_boolean()) {
        bad_field(key);
    }
    out = object.at(key).get<bool>();
}

void read_optional_number(const json& object, const char* key, std::optional<double>& out) {
    if (!object.contains(key) || object.at(key).is_null()) {
        return;
    }
    if (!object.at(key).is_number()) {
        bad_field(key);
    }
    out = object.at(key).get<double>();
}

void read_optional_object(const json& object, const char* key, std::optional<json>& out) {
    if (!object.contains(key) || object.at(key).is_null()) {
        return;
    }
    if (!object.at(key).is_object()) {
        bad_field(key);
    }
    out = object.at(key);
}

void read_transform(const json& object, LayerTransform& transform) {
    read_pair(object, "origin", transform.origin_x, transform.origin_y);
    read_pair(object, "size", transform.width, transform.height);
    if (object.contains("rotation") && !object.at("rotation").is_null()) {
        if (!object.at("rotation").is_number()) {
            bad_field("rotation");
        }
        transform.rotation = object.at("rotation").get<double>();
    }
    if (object.contains("flipX") && !object.at("flipX").is_null()) {
        if (!object.at("flipX").is_boolean()) {
            bad_field("flipX");
        }
        transform.flip_x = object.at("flipX").get<bool>();
    }
    if (object.contains("flipY") && !object.at("flipY").is_null()) {
        if (!object.at("flipY").is_boolean()) {
            bad_field("flipY");
        }
        transform.flip_y = object.at("flipY").get<bool>();
    }
    if (object.contains("sampling") && !object.at("sampling").is_null()) {
        if (!object.at("sampling").is_string() ||
            !sampling_from_string(object.at("sampling").get<std::string>(), transform.sampling)) {
            bad_field("sampling");
        }
    }
}

void write_transform(json& object, const LayerTransform& transform) {
    object["origin"] = json::array({transform.origin_x, transform.origin_y});
    object["size"] = json::array({transform.width, transform.height});
    object["rotation"] = transform.rotation;
    object["flipX"] = transform.flip_x;
    object["flipY"] = transform.flip_y;
    object["sampling"] = std::string(to_string(transform.sampling));
}

}  // namespace

void to_json(json& object, const LayerTransform& transform) {
    write_transform(object, transform);
}

void from_json(const json& object, LayerTransform& transform) {
    if (!object.is_object()) {
        bad_field("transform");
    }
    read_transform(object, transform);
}

void to_json(json& object, const CanvasGuide& guide) {
    object["id"] = guide.id;
    object["axis"] = std::string(to_string(guide.axis));
    object["position"] = guide.position;
}

void from_json(const json& object, CanvasGuide& guide) {
    guide.id = required_uuid(object, "id");
    const std::string axis = required_string(object, "axis");
    if (!guide_axis_from_string(axis, guide.axis)) {
        bad_field("axis");
    }
    guide.position = required_number(object, "position");
}

void to_json(json& object, const ProjectLayerRecord& record) {
    object["id"] = record.id;
    object["name"] = record.name;
    object["isVisible"] = record.is_visible;
    write_transform(object["transform"], record.transform);
    if (record.image_file) {
        object["imageFile"] = *record.image_file;
    }
    if (record.parent_id) {
        object["parentID"] = *record.parent_id;
    }
    if (record.is_group) {
        object["isGroup"] = *record.is_group;
    }
    if (record.opacity) {
        object["opacity"] = *record.opacity;
    }
    if (record.blend_mode) {
        object["blendMode"] = std::string(to_string(*record.blend_mode));
    }
    if (record.mask_file) {
        object["maskFile"] = *record.mask_file;
    }
    if (record.mask_enabled) {
        object["maskEnabled"] = *record.mask_enabled;
    }
    if (record.mask_source_id) {
        object["maskSourceID"] = *record.mask_source_id;
    }
    if (record.mask_placement) {
        write_transform(object["maskPlacement"], *record.mask_placement);
    }
    if (record.mask_linked) {
        object["maskLinked"] = *record.mask_linked;
    }
    if (record.adjustment) {
        object["adjustment"] = *record.adjustment;
    }
    if (record.effects) {
        object["effects"] = *record.effects;
    }
    if (record.text) {
        object["text"] = *record.text;
    }
    if (record.shape) {
        object["shape"] = *record.shape;
    }
}

void from_json(const json& object, ProjectLayerRecord& record) {
    if (!object.is_object()) {
        bad_field("layer");
    }
    record.id = required_uuid(object, "id");
    record.name = required_string(object, "name");
    record.is_visible = required_bool(object, "isVisible");
    from_json(required(object, "transform"), record.transform);
    read_optional_string(object, "imageFile", record.image_file);
    read_optional_uuid(object, "parentID", record.parent_id);
    read_optional_bool(object, "isGroup", record.is_group);
    read_optional_number(object, "opacity", record.opacity);
    if (object.contains("blendMode") && !object.at("blendMode").is_null()) {
        if (!object.at("blendMode").is_string() ||
            !blend_mode_from_string(object.at("blendMode").get<std::string>(), record.blend_mode.emplace())) {
            bad_field("blendMode");
        }
    }
    read_optional_string(object, "maskFile", record.mask_file);
    read_optional_bool(object, "maskEnabled", record.mask_enabled);
    read_optional_uuid(object, "maskSourceID", record.mask_source_id);
    if (object.contains("maskPlacement") && !object.at("maskPlacement").is_null()) {
        LayerTransform placement;
        from_json(object.at("maskPlacement"), placement);
        record.mask_placement = placement;
    }
    read_optional_bool(object, "maskLinked", record.mask_linked);
    read_optional_object(object, "adjustment", record.adjustment);
    read_optional_object(object, "effects", record.effects);
    read_optional_object(object, "text", record.text);
    read_optional_object(object, "shape", record.shape);
}

void to_json(json& object, const ProjectManifest& manifest) {
    object["format"] = manifest.format;
    object["version"] = manifest.version;
    object["colorSpace"] = manifest.color_space;
    if (manifest.resolution) {
        object["resolution"] = *manifest.resolution;
    }
    object["documentID"] = manifest.document_id;
    object["width"] = manifest.width;
    object["height"] = manifest.height;
    if (manifest.active_layer_id) {
        object["activeLayerID"] = *manifest.active_layer_id;
    }
    object["layers"] = manifest.layers;
    if (manifest.guides) {
        object["guides"] = *manifest.guides;
    }
}

void from_json(const json& object, ProjectManifest& manifest) {
    if (!object.is_object()) {
        bad_field("manifest");
    }
    if (object.contains("format") && object.at("format").is_string()) {
        manifest.format = object.at("format").get<std::string>();
    }
    if (object.contains("version") && object.at("version").is_number_integer()) {
        manifest.version = object.at("version").get<int>();
    }
    if (object.contains("colorSpace") && object.at("colorSpace").is_string()) {
        manifest.color_space = object.at("colorSpace").get<std::string>();
    }
    read_optional_number(object, "resolution", manifest.resolution);
    manifest.document_id = required_uuid(object, "documentID");
    manifest.width = required_int(object, "width");
    manifest.height = required_int(object, "height");
    read_optional_uuid(object, "activeLayerID", manifest.active_layer_id);
    const json& layers = required(object, "layers");
    if (!layers.is_array()) {
        bad_field("layers");
    }
    manifest.layers.clear();
    manifest.layers.reserve(layers.size());
    for (const json& layer : layers) {
        ProjectLayerRecord record;
        from_json(layer, record);
        manifest.layers.push_back(std::move(record));
    }
    if (object.contains("guides") && !object.at("guides").is_null()) {
        if (!object.at("guides").is_array()) {
            bad_field("guides");
        }
        std::vector<CanvasGuide> guides;
        guides.reserve(object.at("guides").size());
        for (const json& guide : object.at("guides")) {
            CanvasGuide parsed;
            from_json(guide, parsed);
            guides.push_back(std::move(parsed));
        }
        manifest.guides = std::move(guides);
    }
}

ProjectManifest parse_manifest(const std::string& text) {
    json parsed;
    try {
        parsed = json::parse(text);
    } catch (const json::exception&) {
        throw ProjectError(ProjectErrorCode::invalid, "The project manifest is not valid JSON.");
    }
    ProjectManifest manifest;
    from_json(parsed, manifest);
    return manifest;
}

std::string serialize_manifest(const ProjectManifest& manifest) {
    json object = manifest;
    return object.dump(2);
}

ManifestHeader parse_manifest_header(const std::string& text) {
    json parsed;
    try {
        parsed = json::parse(text);
    } catch (const json::exception&) {
        throw ProjectError(ProjectErrorCode::invalid, "The project manifest is not valid JSON.");
    }
    ManifestHeader header;
    if (parsed.is_object()) {
        if (parsed.contains("format") && parsed.at("format").is_string()) {
            header.format = parsed.at("format").get<std::string>();
        }
        if (parsed.contains("version") && parsed.at("version").is_number_integer()) {
            header.version = parsed.at("version").get<int>();
        }
    }
    return header;
}

}  // namespace compositor::model
