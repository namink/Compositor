#include <gtest/gtest.h>

#include <string>

#include "compositor/model/errors.hpp"
#include "compositor/model/manifest_json.hpp"
#include "compositor/model/uuid.hpp"
#include "support/fixtures.hpp"

namespace {

using namespace compositor::model;

TEST(UuidTest, ValidatesAndNormalizes) {
    EXPECT_TRUE(is_valid_uuid("6F1D3C2A-0B7E-4E8A-9C4D-2A1B3C4D5E6F"));
    EXPECT_TRUE(is_valid_uuid("6f1d3c2a-0b7e-4e8a-9c4d-2a1b3c4d5e6f"));
    EXPECT_FALSE(is_valid_uuid("6F1D3C2A0B7E4E8A9C4D2A1B3C4D5E6F"));
    EXPECT_FALSE(is_valid_uuid("6F1D3C2A-0B7E-4E8A-9C4D-2A1B3C4D5E6Z"));
    EXPECT_FALSE(is_valid_uuid(""));

    EXPECT_EQ(uppercase_uuid("6f1d3c2a-0b7e-4e8a-9c4d-2a1b3c4d5e6f"), "6F1D3C2A-0B7E-4E8A-9C4D-2A1B3C4D5E6F");
    EXPECT_TRUE(is_valid_uuid(generate_uuid()));
}

TEST(TransformJsonTest, RoundTripsAllFields) {
    LayerTransform transform;
    transform.origin_x = 12;
    transform.origin_y = -8;
    transform.width = 1920;
    transform.height = 1080;
    transform.rotation = 45;
    transform.flip_x = true;
    transform.flip_y = false;
    transform.sampling = LayerSampling::nearest;

    const nlohmann::json object = transform;
    LayerTransform parsed;
    from_json(object, parsed);

    EXPECT_EQ(parsed.origin_x, 12);
    EXPECT_EQ(parsed.origin_y, -8);
    EXPECT_EQ(parsed.width, 1920);
    EXPECT_EQ(parsed.height, 1080);
    EXPECT_EQ(parsed.rotation, 45);
    EXPECT_TRUE(parsed.flip_x);
    EXPECT_FALSE(parsed.flip_y);
    EXPECT_EQ(parsed.sampling, LayerSampling::nearest);
}

TEST(TransformJsonTest, OptionalFieldsDefaultToUprightHighQuality) {
    const nlohmann::json object = {{"origin", {0, 0}}, {"size", {10, 20}}};
    LayerTransform parsed;
    from_json(object, parsed);
    EXPECT_EQ(parsed.rotation, 0);
    EXPECT_FALSE(parsed.flip_x);
    EXPECT_FALSE(parsed.flip_y);
    EXPECT_EQ(parsed.sampling, LayerSampling::high);
}

TEST(TransformJsonTest, RejectsMalformedOrigin) {
    const nlohmann::json object = {{"origin", {0}}, {"size", {10, 20}}};
    LayerTransform parsed;
    EXPECT_THROW(from_json(object, parsed), ProjectError);
}

TEST(ManifestJsonTest, RoundTripsAFullManifest) {
    ProjectManifest manifest = comp_test::single_layer_manifest(1920, 1080);
    manifest.resolution = 300;
    manifest.layers[0].opacity = 0.5;
    manifest.layers[0].blend_mode = LayerBlendMode::multiply;
    manifest.layers[0].mask_file = std::string(comp_test::kLayerA) + ".mask.png";
    manifest.layers[0].mask_enabled = true;

    const nlohmann::json object = manifest;
    ProjectManifest parsed;
    from_json(object, parsed);

    ASSERT_EQ(parsed.layers.size(), 1U);
    EXPECT_EQ(parsed.document_id, comp_test::kDocId);
    EXPECT_EQ(parsed.width, 1920);
    EXPECT_EQ(parsed.version, kCurrentFormatVersion);
    ASSERT_TRUE(parsed.resolution.has_value());
    EXPECT_EQ(*parsed.resolution, 300);
    ASSERT_TRUE(parsed.layers[0].opacity.has_value());
    EXPECT_DOUBLE_EQ(*parsed.layers[0].opacity, 0.5);
    ASSERT_TRUE(parsed.layers[0].blend_mode.has_value());
    EXPECT_EQ(*parsed.layers[0].blend_mode, LayerBlendMode::multiply);
    EXPECT_EQ(parsed.layers[0].mask_file, std::string(comp_test::kLayerA) + ".mask.png");
}

TEST(ManifestJsonTest, SerializeParseIsStable) {
    const ProjectManifest manifest = comp_test::single_layer_manifest(8, 6);
    const std::string text = serialize_manifest(manifest);
    const ProjectManifest reparsed = parse_manifest(text);
    EXPECT_EQ(serialize_manifest(reparsed), text);
}

TEST(ManifestJsonTest, UppercasesIdsAndReferences) {
    const std::string text = R"({"format":"com.compositor.project","version":11,"colorSpace":"sRGB",)"
                             R"("documentID":"0c5e7a91-3b2d-4f6a-8e1c-9d0b7a6f5e4d","width":4,"height":4,)"
                             R"("activeLayerID":"6f1d3c2a-0b7e-4e8a-9c4d-2a1b3c4d5e6f","layers":[)"
                             R"({"id":"6f1d3c2a-0b7e-4e8a-9c4d-2a1b3c4d5e6f","name":"A","isVisible":true,)"
                             R"("imageFile":"6f1d3c2a-0b7e-4e8a-9c4d-2a1b3c4d5e6f.png",)"
                             R"("transform":{"origin":[0,0],"size":[4,4]}}]})";
    const ProjectManifest manifest = parse_manifest(text);
    EXPECT_EQ(manifest.document_id, comp_test::kDocId);
    EXPECT_EQ(manifest.layers[0].id, comp_test::kLayerA);
    EXPECT_EQ(manifest.active_layer_id, std::optional<std::string>(comp_test::kLayerA));
}

TEST(ManifestJsonTest, HeaderPeekReadsFormatAndVersion) {
    const ProjectManifest manifest = comp_test::single_layer_manifest(4, 4, 11);
    const ManifestHeader header = parse_manifest_header(serialize_manifest(manifest));
    EXPECT_EQ(header.format, kFormatIdentifier);
    EXPECT_EQ(header.version, 11);
}

TEST(ManifestJsonTest, InvalidJsonThrowsInvalid) {
    EXPECT_THROW((void)parse_manifest("{not json"), ProjectError);
    EXPECT_THROW((void)parse_manifest_header("{not json"), ProjectError);
}

TEST(BlendModeTest, EveryModeRoundTripsThroughItsName) {
    for (const LayerBlendMode mode : all_blend_modes()) {
        LayerBlendMode parsed = LayerBlendMode::normal;
        ASSERT_TRUE(blend_mode_from_string(to_string(mode), parsed));
        EXPECT_EQ(parsed, mode);
    }
    EXPECT_EQ(all_blend_modes().size(), 24U);
    LayerBlendMode unused = LayerBlendMode::normal;
    EXPECT_FALSE(blend_mode_from_string("Darker Color", unused));
}

}  // namespace
