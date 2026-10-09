#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "compositor/model/errors.hpp"
#include "compositor/model/project_store.hpp"
#include "compositor/model/uuid.hpp"
#include "support/fixtures.hpp"

namespace fs = std::filesystem;

namespace {

using namespace compositor::model;
using comp_test::ProjectManifest;
using comp_test::ProjectSnapshot;

/// A scratch directory that removes itself when the test ends.
struct TempDir {
    fs::path path;
    TempDir() : path(fs::temp_directory_path() / ("comp-test-" + generate_uuid())) { fs::create_directories(path); }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
};

void write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

std::optional<ProjectErrorCode> load_code(const fs::path& package) {
    try {
        (void)ProjectStore::load(package);
        return std::nullopt;
    } catch (const ProjectError& error) {
        return error.code();
    }
}

TEST(ProjectStoreTest, SaveThenLoadRoundTrips) {
    TempDir temp;
    const fs::path package = temp.path / "demo.comp";
    const ProjectManifest manifest = comp_test::single_layer_manifest(8, 6);
    const ProjectSnapshot snapshot = comp_test::snapshot_with_images(manifest, 6);

    ProjectStore::save(snapshot, package);

    ASSERT_TRUE(fs::is_regular_file(package / "manifest.json"));
    ASSERT_TRUE(fs::is_regular_file(package / "images" / (std::string(comp_test::kLayerA) + ".png")));

    const ProjectSnapshot loaded = ProjectStore::load(package);
    EXPECT_EQ(loaded.manifest.document_id, comp_test::kDocId);
    EXPECT_EQ(loaded.manifest.width, 8);
    EXPECT_EQ(loaded.manifest.height, 6);
    ASSERT_EQ(loaded.images.size(), 1U);
    const ImageAsset& asset = loaded.images.at(comp_test::kLayerA);
    EXPECT_EQ(asset.width, 8);
    EXPECT_EQ(asset.height, 6);
    EXPECT_EQ(asset.png, snapshot.images.at(comp_test::kLayerA).png);
}

TEST(ProjectStoreTest, SaveReplacesAnExistingPackage) {
    TempDir temp;
    const fs::path package = temp.path / "demo.comp";

    ProjectManifest first = comp_test::single_layer_manifest(4, 4);
    ProjectStore::save(comp_test::snapshot_with_images(first), package);

    ProjectManifest second = comp_test::single_layer_manifest(16, 16);
    second.layers[0].name = "Renamed";
    ProjectStore::save(comp_test::snapshot_with_images(second), package);

    const ProjectSnapshot loaded = ProjectStore::load(package);
    EXPECT_EQ(loaded.manifest.width, 16);
    ASSERT_EQ(loaded.manifest.layers.size(), 1U);
    EXPECT_EQ(loaded.manifest.layers[0].name, "Renamed");
    // No staging directory is left behind.
    EXPECT_FALSE(fs::exists(temp.path / (package.filename().string() + ".bak")));
}

TEST(ProjectStoreTest, LoadRejectsMissingManifest) {
    TempDir temp;
    const fs::path package = temp.path / "empty.comp";
    fs::create_directories(package);
    EXPECT_EQ(load_code(package), ProjectErrorCode::invalid);
}

TEST(ProjectStoreTest, LoadRejectsUnsupportedVersion) {
    TempDir temp;
    const fs::path package = temp.path / "future.comp";
    fs::create_directories(package);
    write_text(package / "manifest.json", R"({"format":"com.compositor.project","version":99,"colorSpace":"sRGB",)"
                                          R"("documentID":"0C5E7A91-3B2D-4F6A-8E1C-9D0B7A6F5E4D",)"
                                          R"("width":4,"height":4,"layers":[]})");
    EXPECT_EQ(load_code(package), ProjectErrorCode::unsupported_version);
}

TEST(ProjectStoreTest, LoadRejectsMissingImage) {
    TempDir temp;
    const fs::path package = temp.path / "demo.comp";
    ProjectStore::save(comp_test::snapshot_with_images(comp_test::single_layer_manifest(4, 4)), package);
    fs::remove(package / "images" / (std::string(comp_test::kLayerA) + ".png"));
    EXPECT_EQ(load_code(package), ProjectErrorCode::missing_image);
}

TEST(ProjectStoreTest, LoadRejectsDamagedImage) {
    TempDir temp;
    const fs::path package = temp.path / "demo.comp";
    ProjectStore::save(comp_test::snapshot_with_images(comp_test::single_layer_manifest(4, 4)), package);
    write_text(package / "images" / (std::string(comp_test::kLayerA) + ".png"), "not a png");
    EXPECT_EQ(load_code(package), ProjectErrorCode::missing_image);
}

TEST(ProjectStoreTest, SaveRejectsAssetMissingFromSnapshot) {
    TempDir temp;
    ProjectSnapshot snapshot;
    snapshot.manifest = comp_test::single_layer_manifest(4, 4);
    EXPECT_THROW(ProjectStore::save(snapshot, temp.path / "demo.comp"), ProjectError);
}

TEST(ProjectStoreTest, RoundTripsGuidesAndNestedObjects) {
    TempDir temp;
    const fs::path package = temp.path / "nested.comp";
    ProjectManifest manifest = comp_test::single_layer_manifest(4, 4);
    manifest.guides = std::vector<CanvasGuide>{{comp_test::kFolder, CanvasGuide::Axis::horizontal, 12.5},
                                               {comp_test::kLayerB, CanvasGuide::Axis::vertical, 7.0}};
    manifest.layers[0].effects = nlohmann::json::object({{"stroke", nlohmann::json::object({{"size", 3}})}});
    ProjectStore::save(comp_test::snapshot_with_images(manifest), package);

    const ProjectSnapshot loaded = ProjectStore::load(package);
    ASSERT_TRUE(loaded.manifest.guides.has_value());
    EXPECT_EQ(loaded.manifest.guides->size(), 2U);
    EXPECT_DOUBLE_EQ(loaded.manifest.guides->at(0).position, 12.5);
    ASSERT_TRUE(loaded.manifest.layers[0].effects.has_value());
    EXPECT_EQ(loaded.manifest.layers[0].effects->at("stroke").at("size").get<int>(), 3);
}

}  // namespace
