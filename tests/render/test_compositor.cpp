#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <map>
#include <string>

#include "compositor/model/manifest.hpp"
#include "compositor/render/compositor.hpp"
#include "compositor/render/render_order.hpp"
#include "support/fixtures.hpp"

namespace {

using namespace compositor::model;
using namespace compositor::render;
using comp_test::kLayerA;
using comp_test::kLayerB;

std::uint8_t premul(std::uint8_t channel, std::uint8_t alpha) {
    return static_cast<std::uint8_t>(std::lround(static_cast<double>(channel) * alpha / 255.0));
}

RgbaSurface solid(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
    RgbaSurface surface(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            surface.set(x, y, premul(r, a), premul(g, a), premul(b, a), a);
        }
    }
    return surface;
}

/// The four bytes of pixel (x, y).
void expect_pixel(const RgbaSurface& surface, int x, int y, int r, int g, int b, int a, int tolerance = 1) {
    const std::size_t at = surface.offset(x, y);
    const std::uint8_t* p = surface.data() + at;
    EXPECT_NEAR(p[0], r, tolerance) << "at (" << x << "," << y << ") red";
    EXPECT_NEAR(p[1], g, tolerance) << "at (" << x << "," << y << ") green";
    EXPECT_NEAR(p[2], b, tolerance) << "at (" << x << "," << y << ") blue";
    EXPECT_NEAR(p[3], a, tolerance) << "at (" << x << "," << y << ") alpha";
}

ProjectLayerRecord group_layer(const std::string& id, const std::string& name, double opacity) {
    ProjectLayerRecord record;
    record.id = id;
    record.name = name;
    record.is_visible = true;
    record.transform = comp_test::full_canvas_transform(4, 4);
    record.is_group = true;
    record.opacity = opacity;
    return record;
}

TEST(CompositeTest, SingleOpaqueLayerCopiesThrough) {
    const ProjectManifest manifest = comp_test::single_layer_manifest(4, 4);
    const std::map<std::string, RgbaSurface> images{{kLayerA, solid(4, 4, 10, 20, 30)}};
    const RgbaSurface result = composite_document(manifest, images, {});
    expect_pixel(result, 0, 0, 10, 20, 30, 255);
    expect_pixel(result, 3, 3, 10, 20, 30, 255);
}

TEST(CompositeTest, SourceOverBlendsWithAlpha) {
    ProjectManifest manifest = comp_test::single_layer_manifest(2, 2);
    manifest.layers[0].transform = comp_test::full_canvas_transform(2, 2);
    manifest.layers.push_back(comp_test::image_layer(kLayerB, "Top", 2, 2));
    const std::map<std::string, RgbaSurface> images{{kLayerA, solid(2, 2, 255, 0, 0)},
                                                    {kLayerB, solid(2, 2, 255, 255, 255, 128)}};
    const RgbaSurface result = composite_document(manifest, images, {});
    // White at half alpha over red: the red channel stays full, the others take about half the white.
    expect_pixel(result, 0, 0, 255, 128, 128, 255, 2);
}

TEST(CompositeTest, MultiplyDarkens) {
    ProjectManifest manifest = comp_test::single_layer_manifest(2, 2);
    manifest.layers.push_back(comp_test::image_layer(kLayerB, "Top", 2, 2));
    manifest.layers[1].blend_mode = LayerBlendMode::multiply;
    const std::map<std::string, RgbaSurface> images{{kLayerA, solid(2, 2, 128, 128, 128)},
                                                    {kLayerB, solid(2, 2, 128, 128, 128)}};
    const RgbaSurface result = composite_document(manifest, images, {});
    expect_pixel(result, 1, 1, 64, 64, 64, 255, 2);
}

TEST(CompositeTest, LayerMaskHidesWhereBlack) {
    ProjectManifest manifest = comp_test::single_layer_manifest(4, 2);
    manifest.layers[0].mask_file = std::string(kLayerA) + ".mask.png";
    manifest.layers[0].mask_enabled = true;
    RgbaSurface mask(4, 2);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 4; ++x) {
            const std::uint8_t coverage = x < 2 ? 0 : 255;  // left half hides, right half reveals
            mask.set(x, y, coverage, coverage, coverage, 255);
        }
    }
    const std::map<std::string, RgbaSurface> images{{kLayerA, solid(4, 2, 0, 255, 0)}};
    const std::map<std::string, RgbaSurface> masks{{kLayerA, mask}};
    const RgbaSurface result = composite_document(manifest, images, masks);
    expect_pixel(result, 0, 0, 0, 0, 0, 0);
    expect_pixel(result, 2, 0, 0, 255, 0, 255);
    expect_pixel(result, 3, 1, 0, 255, 0, 255);
}

TEST(CompositeTest, HiddenLayerIsSkipped) {
    ProjectManifest manifest = comp_test::single_layer_manifest(2, 2);
    manifest.layers[0].is_visible = false;
    const std::map<std::string, RgbaSurface> images{{kLayerA, solid(2, 2, 10, 20, 30)}};
    const RgbaSurface result = composite_document(manifest, images, {});
    expect_pixel(result, 0, 0, 0, 0, 0, 0);
}

TEST(CompositeTest, FolderOpacityMultipliesIntoChildren) {
    ProjectManifest manifest = comp_test::single_layer_manifest(2, 2);
    manifest.layers.clear();
    ProjectLayerRecord folder = group_layer(comp_test::kFolder, "Folder 1", 0.5);
    ProjectLayerRecord child = comp_test::image_layer(kLayerB, "Child", 2, 2);
    child.parent_id = comp_test::kFolder;
    manifest.layers = {folder, child};
    manifest.active_layer_id.reset();
    const std::map<std::string, RgbaSurface> images{{kLayerB, solid(2, 2, 0, 255, 0)}};
    const RgbaSurface result = composite_document(manifest, images, {});
    // Half opacity green: premultiplied green stays near full, alpha halves.
    expect_pixel(result, 0, 0, 0, 128, 0, 128, 2);
}

TEST(CompositeTest, ClippingMaskUsesTheBaseAlpha) {
    ProjectManifest manifest = comp_test::single_layer_manifest(4, 2);
    manifest.layers.push_back(comp_test::image_layer(kLayerB, "Top", 4, 2));
    manifest.layers[1].mask_source_id = kLayerA;
    manifest.layers[0].mask_file = std::string(kLayerA) + ".mask.png";
    manifest.layers[0].mask_enabled = true;
    RgbaSurface mask(4, 2);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 4; ++x) {
            const std::uint8_t coverage = x < 2 ? 0 : 255;  // base hidden on the left
            mask.set(x, y, coverage, coverage, coverage, 255);
        }
    }
    const std::map<std::string, RgbaSurface> images{{kLayerA, solid(4, 2, 255, 0, 0)},
                                                    {kLayerB, solid(4, 2, 0, 0, 255)}};
    const std::map<std::string, RgbaSurface> masks{{kLayerA, mask}};
    const RgbaSurface result = composite_document(manifest, images, masks);
    // The clipped top layer only shows where the base is: transparent on the left, blue on the right.
    expect_pixel(result, 0, 0, 0, 0, 0, 0);
    expect_pixel(result, 3, 0, 0, 0, 255, 255);
}

TEST(RenderOrderTest, DrawsBottomToTopWithEffectiveOpacity) {
    ProjectManifest manifest = comp_test::single_layer_manifest(2, 2);
    manifest.layers.clear();
    ProjectLayerRecord bottom = comp_test::image_layer(kLayerA, "Bottom", 2, 2);
    ProjectLayerRecord folder = group_layer(comp_test::kFolder, "Folder 1", 0.5);
    ProjectLayerRecord child = comp_test::image_layer(kLayerB, "Child", 2, 2);
    child.parent_id = comp_test::kFolder;
    manifest.layers = {bottom, folder, child};

    const std::vector<DrawItem> items = resolve_draw_items(manifest);
    ASSERT_EQ(items.size(), 2U);
    EXPECT_EQ(items[0].layer->id, kLayerA);
    EXPECT_DOUBLE_EQ(items[0].opacity, 1.0);
    EXPECT_EQ(items[1].layer->id, kLayerB);
    EXPECT_DOUBLE_EQ(items[1].opacity, 0.5);
}

TEST(RenderOrderTest, HiddenFolderDropsItsSubtree) {
    ProjectManifest manifest = comp_test::single_layer_manifest(2, 2);
    manifest.layers.clear();
    ProjectLayerRecord folder = group_layer(comp_test::kFolder, "Folder 1", 1.0);
    folder.is_visible = false;
    ProjectLayerRecord child = comp_test::image_layer(kLayerB, "Child", 2, 2);
    child.parent_id = comp_test::kFolder;
    manifest.layers = {folder, child};
    EXPECT_TRUE(resolve_draw_items(manifest).empty());
}

}  // namespace
