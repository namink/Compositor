#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <string>

#include "compositor/io/image_codec.hpp"
#include "compositor/model/project_store.hpp"
#include "compositor/model/uuid.hpp"
#include "compositor/render/compositor.hpp"
#include "support/fixtures.hpp"
#include "support/png_builder.hpp"

namespace fs = std::filesystem;

namespace {

using namespace compositor::model;
using namespace compositor::render;
using comp_test::kLayerA;

/// A scratch directory that removes itself when the test ends.
struct TempDir {
    fs::path path;
    TempDir() : path(fs::temp_directory_path() / ("comp-e2e-" + generate_uuid())) { fs::create_directories(path); }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
};

std::uint8_t premul(std::uint8_t channel, std::uint8_t alpha) {
    return static_cast<std::uint8_t>((static_cast<unsigned>(channel) * alpha + 127U) / 255U);
}

RgbaSurface solid(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    RgbaSurface surface(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            surface.set(x, y, premul(r, a), premul(g, a), premul(b, a), a);
        }
    }
    return surface;
}

ImageAsset asset_from(const RgbaSurface& surface) {
    ImageAsset asset;
    asset.width = surface.width();
    asset.height = surface.height();
    asset.png = compositor::io::encode_png(surface);
    return asset;
}

/// Decode every image the loaded snapshot names, keyed by layer id.
std::map<std::string, RgbaSurface> decode_all(const std::map<std::string, ImageAsset>& assets) {
    std::map<std::string, RgbaSurface> surfaces;
    for (const auto& [id, asset] : assets) {
        surfaces.emplace(id, compositor::io::decode_image(asset.png));
    }
    return surfaces;
}

void expect_pixel(const RgbaSurface& surface, int x, int y, int r, int g, int b, int a, int tolerance = 1) {
    const std::uint8_t* p = surface.data() + surface.offset(x, y);
    EXPECT_NEAR(p[0], r, tolerance);
    EXPECT_NEAR(p[1], g, tolerance);
    EXPECT_NEAR(p[2], b, tolerance);
    EXPECT_NEAR(p[3], a, tolerance);
}

TEST(CompPipelineTest, SaveLoadDecodeCompositeRoundTrips) {
    TempDir temp;
    const fs::path package = temp.path / "demo.comp";

    ProjectSnapshot snapshot;
    snapshot.manifest = comp_test::single_layer_manifest(4, 4);
    snapshot.images[kLayerA] = asset_from(solid(4, 4, 200, 40, 10, 255));
    ProjectStore::save(snapshot, package);

    const ProjectSnapshot loaded = ProjectStore::load(package);
    const RgbaSurface result = composite_document(loaded.manifest, decode_all(loaded.images), decode_all(loaded.masks));
    expect_pixel(result, 0, 0, 200, 40, 10, 255, 2);
    expect_pixel(result, 3, 3, 200, 40, 10, 255, 2);
}

TEST(CompPipelineTest, MaskedLayerIsHiddenAfterRoundTrip) {
    TempDir temp;
    const fs::path package = temp.path / "masked.comp";

    ProjectSnapshot snapshot;
    snapshot.manifest = comp_test::single_layer_manifest(4, 4);
    snapshot.manifest.layers[0].mask_file = std::string(kLayerA) + ".mask.png";
    snapshot.manifest.layers[0].mask_enabled = true;
    snapshot.images[kLayerA] = asset_from(solid(4, 4, 0, 255, 0, 255));
    // An all-black mask hides the layer completely.
    ImageAsset mask;
    mask.width = 4;
    mask.height = 4;
    mask.png = comp_test::make_png(4, 4, 0, 0);
    snapshot.masks[kLayerA] = mask;
    ProjectStore::save(snapshot, package);

    const ProjectSnapshot loaded = ProjectStore::load(package);
    const RgbaSurface result = composite_document(loaded.manifest, decode_all(loaded.images), decode_all(loaded.masks));
    expect_pixel(result, 0, 0, 0, 0, 0, 0, 0);
    expect_pixel(result, 3, 3, 0, 0, 0, 0, 0);
}

}  // namespace
