#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "compositor/model/errors.hpp"
#include "compositor/model/limits.hpp"
#include "compositor/model/validation.hpp"
#include "support/fixtures.hpp"

namespace {

using namespace compositor::model;
using comp_test::ProjectLayerRecord;
using comp_test::ProjectManifest;

std::optional<ProjectErrorCode> validate_code(const ProjectManifest& manifest) {
    try {
        validate_manifest(manifest);
        return std::nullopt;
    } catch (const ProjectError& error) {
        return error.code();
    }
}

void expect_code(const ProjectManifest& manifest, ProjectErrorCode expected) {
    const std::optional<ProjectErrorCode> actual = validate_code(manifest);
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(*actual, expected);
}

ProjectLayerRecord group_layer(const std::string& id, const std::string& name) {
    ProjectLayerRecord record;
    record.id = id;
    record.name = name;
    record.is_visible = true;
    record.transform = comp_test::full_canvas_transform(4, 4);
    record.is_group = true;
    return record;
}

TEST(ValidationBasicsTest, MinimalManifestsPass) {
    EXPECT_FALSE(validate_code(comp_test::single_layer_manifest(4, 4, 1)).has_value());
    EXPECT_FALSE(validate_code(comp_test::single_layer_manifest(4, 4, 11)).has_value());
}

TEST(ValidationBasicsTest, IdentityRules) {
    ProjectManifest manifest = comp_test::single_layer_manifest();
    manifest.format = "com.example.other";
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest = comp_test::single_layer_manifest();
    manifest.color_space = "Display P3";
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest = comp_test::single_layer_manifest();
    manifest.version = 0;
    expect_code(manifest, ProjectErrorCode::unsupported_version);

    manifest = comp_test::single_layer_manifest();
    manifest.version = 12;
    expect_code(manifest, ProjectErrorCode::unsupported_version);

    manifest = comp_test::single_layer_manifest();
    manifest.resolution = 0.0;
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest = comp_test::single_layer_manifest();
    manifest.resolution = 9601.0;
    expect_code(manifest, ProjectErrorCode::invalid);
}

TEST(ValidationBasicsTest, SizeLimits) {
    ProjectManifest manifest = comp_test::single_layer_manifest();
    manifest.width = 0;
    expect_code(manifest, ProjectErrorCode::too_large);

    manifest = comp_test::single_layer_manifest();
    manifest.width = DocumentLimits::kMaxSide + 1;
    expect_code(manifest, ProjectErrorCode::too_large);

    manifest = comp_test::single_layer_manifest();
    manifest.layers.clear();
    for (std::size_t i = 0; i <= DocumentLimits::kMaxLayers; ++i) {
        manifest.layers.push_back(comp_test::image_layer(comp_test::kLayerA, "L", 4, 4));
    }
    expect_code(manifest, ProjectErrorCode::too_large);
}

TEST(ValidationLayerTest, IdentityAndNaming) {
    ProjectManifest manifest = comp_test::single_layer_manifest();
    manifest.layers[0].name = "   ";
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest = comp_test::single_layer_manifest();
    manifest.layers[0].image_file = "wrong.png";
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest = comp_test::single_layer_manifest();
    manifest.layers[0].transform.width = 0;
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest = comp_test::single_layer_manifest();
    manifest.active_layer_id = comp_test::kLayerB;
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest = comp_test::single_layer_manifest();
    manifest.layers.push_back(manifest.layers[0]);
    expect_code(manifest, ProjectErrorCode::invalid);
}

TEST(ValidationHierarchyTest, FolderRules) {
    ProjectManifest manifest = comp_test::single_layer_manifest();
    manifest.layers[0].is_group = true;  // a folder must not carry pixels
    expect_code(manifest, ProjectErrorCode::invalid);

    // Missing parent.
    manifest = comp_test::single_layer_manifest();
    manifest.layers[0].parent_id = comp_test::kFolder;
    expect_code(manifest, ProjectErrorCode::invalid);

    // Parent exists but is not a folder.
    manifest = comp_test::single_layer_manifest();
    ProjectLayerRecord other = comp_test::image_layer(comp_test::kLayerB, "B", 4, 4);
    manifest.layers.push_back(other);
    manifest.layers[0].parent_id = comp_test::kLayerB;
    expect_code(manifest, ProjectErrorCode::invalid);

    // Two-folder cycle.
    manifest = comp_test::single_layer_manifest();
    manifest.width = 4;
    manifest.height = 4;
    manifest.layers.clear();
    ProjectLayerRecord a = group_layer(comp_test::kFolder, "A");
    ProjectLayerRecord b = group_layer(comp_test::kLayerB, "B");
    a.parent_id = comp_test::kLayerB;
    b.parent_id = comp_test::kFolder;
    manifest.layers = {a, b};
    manifest.active_layer_id.reset();
    expect_code(manifest, ProjectErrorCode::invalid);
}

TEST(ValidationHierarchyTest, GroupWithChildPasses) {
    ProjectManifest manifest = comp_test::single_layer_manifest();
    ProjectLayerRecord folder = group_layer(comp_test::kFolder, "Folder 1");
    ProjectLayerRecord child = comp_test::image_layer(comp_test::kLayerB, "Child", 4, 4);
    child.parent_id = comp_test::kFolder;
    manifest.layers.push_back(folder);
    manifest.layers.push_back(child);
    EXPECT_FALSE(validate_code(manifest).has_value());
}

TEST(ValidationFeatureTest, MaskVersionAndNaming) {
    ProjectManifest manifest = comp_test::single_layer_manifest(4, 4, 3);
    manifest.layers[0].mask_file = std::string(comp_test::kLayerA) + ".mask.png";
    expect_code(manifest, ProjectErrorCode::invalid);  // masks arrived in version 4

    manifest = comp_test::single_layer_manifest();
    manifest.layers[0].mask_file = "wrong.mask.png";
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest = comp_test::single_layer_manifest();
    manifest.layers[0].mask_enabled = true;  // no maskFile
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest = comp_test::single_layer_manifest();
    manifest.layers[0].mask_file = std::string(comp_test::kLayerA) + ".mask.png";
    manifest.layers[0].mask_enabled = true;
    EXPECT_FALSE(validate_code(manifest).has_value());
}

TEST(ValidationFeatureTest, FolderMaskVersion) {
    ProjectManifest manifest = comp_test::single_layer_manifest(4, 4, 5);
    manifest.layers.push_back(group_layer(comp_test::kFolder, "F"));
    manifest.layers[0].parent_id = comp_test::kFolder;
    manifest.layers[1].mask_file = std::string(comp_test::kFolder) + ".mask.png";
    expect_code(manifest, ProjectErrorCode::invalid);  // folder masks arrived in version 6
}

TEST(ValidationFeatureTest, AppearanceVersionGates) {
    ProjectManifest manifest = comp_test::single_layer_manifest(4, 4, 2);
    manifest.layers[0].opacity = 0.5;
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest = comp_test::single_layer_manifest(4, 4, 2);
    manifest.layers[0].blend_mode = LayerBlendMode::multiply;
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest = comp_test::single_layer_manifest(4, 4, 11);
    manifest.layers[0].opacity = 2.0;
    expect_code(manifest, ProjectErrorCode::invalid);
}

TEST(ValidationFeatureTest, FolderOpacityVersion) {
    ProjectManifest manifest = comp_test::single_layer_manifest(4, 4, 7);
    ProjectLayerRecord folder = group_layer(comp_test::kFolder, "F");
    folder.opacity = 0.5;
    manifest.layers.push_back(folder);
    manifest.layers[0].parent_id = comp_test::kFolder;
    expect_code(manifest, ProjectErrorCode::invalid);  // folder opacity arrived in version 8

    manifest.version = 8;
    EXPECT_FALSE(validate_code(manifest).has_value());
}

TEST(ValidationFeatureTest, TextGates) {
    ProjectManifest manifest = comp_test::single_layer_manifest();
    ProjectLayerRecord folder = group_layer(comp_test::kFolder, "F");
    folder.text = nlohmann::json::object({{"content", "Hi"}});
    manifest.layers.push_back(folder);
    expect_code(manifest, ProjectErrorCode::invalid);  // text cannot sit on a folder

    manifest = comp_test::single_layer_manifest(4, 4, 9);
    manifest.layers[0].text = nlohmann::json::object({{"content", "Hi"}, {"colorRuns", nlohmann::json::array()}});
    expect_code(manifest, ProjectErrorCode::invalid);  // per-letter colors arrived in version 10
}

TEST(ValidationFeatureTest, AdjustmentGates) {
    ProjectManifest manifest = comp_test::single_layer_manifest(4, 4, 6);
    ProjectLayerRecord adjustment;
    adjustment.id = comp_test::kLayerB;
    adjustment.name = "Levels";
    adjustment.transform = comp_test::full_canvas_transform(4, 4);
    adjustment.adjustment = nlohmann::json::object({{"kind", "Levels"}});
    manifest.layers.push_back(adjustment);
    expect_code(manifest, ProjectErrorCode::invalid);  // adjustment layers arrived in version 7

    // An adjustment layer must not carry pixels.
    manifest.version = 11;
    manifest.layers[1].image_file = std::string(comp_test::kLayerB) + ".png";
    expect_code(manifest, ProjectErrorCode::invalid);

    // Sampling kinds arrived in version 9.
    manifest.version = 8;
    manifest.layers[1].image_file.reset();
    manifest.layers[1].adjustment = nlohmann::json::object({{"kind", "Gaussian Blur"}});
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest.version = 9;
    EXPECT_FALSE(validate_code(manifest).has_value());

    // Not allowed on a folder.
    manifest = comp_test::single_layer_manifest();
    ProjectLayerRecord folder = group_layer(comp_test::kFolder, "F");
    folder.adjustment = nlohmann::json::object({{"kind", "Levels"}});
    manifest.layers.push_back(folder);
    expect_code(manifest, ProjectErrorCode::invalid);
}

TEST(ValidationFeatureTest, ClippingMaskGates) {
    // maskSourceID arrived in version 5.
    ProjectManifest manifest = comp_test::single_layer_manifest(4, 4, 4);
    manifest.layers.push_back(comp_test::image_layer(comp_test::kLayerB, "B", 4, 4));
    manifest.layers[0].mask_source_id = comp_test::kLayerB;
    expect_code(manifest, ProjectErrorCode::invalid);

    // A folder cannot supply a live mask.
    manifest = comp_test::single_layer_manifest();
    manifest.layers.push_back(group_layer(comp_test::kFolder, "F"));
    manifest.layers[0].mask_source_id = comp_test::kFolder;
    expect_code(manifest, ProjectErrorCode::invalid);

    // A cycle is rejected.
    manifest = comp_test::single_layer_manifest();
    manifest.layers.push_back(comp_test::image_layer(comp_test::kLayerB, "B", 4, 4));
    manifest.layers[0].mask_source_id = comp_test::kLayerB;
    manifest.layers[1].mask_source_id = comp_test::kLayerA;
    expect_code(manifest, ProjectErrorCode::invalid);

    // A valid clip.
    manifest.layers[1].mask_source_id.reset();
    EXPECT_FALSE(validate_code(manifest).has_value());
}

TEST(ValidationFeatureTest, GuideGates) {
    ProjectManifest manifest = comp_test::single_layer_manifest(4, 4, 7);
    manifest.guides = std::vector<CanvasGuide>{{comp_test::kFolder, CanvasGuide::Axis::horizontal, 10.0}};
    expect_code(manifest, ProjectErrorCode::invalid);  // guides arrived in version 8

    manifest.version = 8;
    EXPECT_FALSE(validate_code(manifest).has_value());

    manifest.guides =
        std::vector<CanvasGuide>{{comp_test::kFolder, CanvasGuide::Axis::horizontal, 10.0},
                                 {comp_test::kFolder, CanvasGuide::Axis::vertical, 20.0}};  // duplicate id
    expect_code(manifest, ProjectErrorCode::invalid);

    manifest.guides = std::vector<CanvasGuide>{
        {comp_test::kFolder, CanvasGuide::Axis::horizontal, DocumentLimits::kMaxGuidePosition + 1.0}};
    expect_code(manifest, ProjectErrorCode::invalid);
}

TEST(ValidationFeatureTest, VersionOneRejectsHierarchy) {
    ProjectManifest manifest = comp_test::single_layer_manifest(4, 4, 1);
    manifest.layers.push_back(group_layer(comp_test::kFolder, "F"));
    manifest.layers[0].parent_id = comp_test::kFolder;
    expect_code(manifest, ProjectErrorCode::invalid);
}

TEST(LimitsTest, DocumentBudgetStaysInRange) {
    const std::int64_t budget = DocumentLimits::document_pixel_budget();
    EXPECT_GE(budget, DocumentLimits::kMaxSurfacePixels);
    EXPECT_LE(budget, 800'000'000);
}

}  // namespace
