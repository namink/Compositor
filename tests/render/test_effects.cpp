#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <string>

#include "compositor/model/manifest.hpp"
#include "compositor/render/compositor.hpp"
#include "compositor/render/effects.hpp"
#include "support/fixtures.hpp"

namespace {

using compositor::render::composite_document;
using compositor::render::has_visible_effects;
using compositor::render::render_layer_effects;
using compositor::render::RenderedEffects;
using compositor::render::RgbaSurface;
using nlohmann::json;

RgbaSurface solid(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    RgbaSurface surface(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            surface.set(x, y, r, g, b, a);
        }
    }
    return surface;
}

std::uint8_t channel_at(const RgbaSurface& surface, int x, int y, int c) {
    return surface.data()[surface.offset(x, y) + static_cast<std::size_t>(c)];
}

TEST(EffectsTest, DetectsVisibleEffects) {
    EXPECT_FALSE(has_visible_effects(json::object()));
    EXPECT_TRUE(has_visible_effects(json{{"colorOverlay", {{"red", 1.0}}}}));
    EXPECT_FALSE(has_visible_effects(json{{"stroke", {{"enabled", false}}}}));
}

TEST(EffectsTest, ColorOverlayPaintsTheShape) {
    const RgbaSurface image = solid(2, 2, 255, 255, 255, 255);
    const json effects = {{"colorOverlay", {{"red", 1.0}, {"green", 0.0}, {"blue", 0.0}, {"opacity", 1.0}}}};
    const RenderedEffects result = render_layer_effects(image, nullptr, effects);
    ASSERT_EQ(result.inset, 2);
    ASSERT_EQ(result.image.width(), 6);
    EXPECT_EQ(channel_at(result.image, 2, 2, 0), 255);
    EXPECT_EQ(channel_at(result.image, 2, 2, 1), 0);
    EXPECT_EQ(channel_at(result.image, 2, 2, 2), 0);
    // Outside the shape stays transparent.
    EXPECT_EQ(channel_at(result.image, 0, 0, 3), 0);
}

TEST(EffectsTest, DropShadowFallsBelow) {
    const RgbaSurface image = solid(2, 2, 255, 255, 255, 255);
    const json effects = {{"shadow",
                           {{"angle", 90.0},
                            {"distance", 2.0},
                            {"blur", 0.0},
                            {"opacity", 1.0},
                            {"red", 0.0},
                            {"green", 0.0},
                            {"blue", 0.0}}}};
    const RenderedEffects result = render_layer_effects(image, nullptr, effects);
    ASSERT_EQ(result.inset, 4);
    // The layer sits at (4,4); the shadow, 2 px down, darkens (4,6).
    EXPECT_EQ(channel_at(result.image, 4, 4, 0), 255);
    EXPECT_EQ(channel_at(result.image, 4, 4, 3), 255);
    EXPECT_LT(channel_at(result.image, 4, 6, 0), 20);
    EXPECT_GT(channel_at(result.image, 4, 6, 3), 200);
}

TEST(EffectsTest, OutsideStrokeRingsTheShape) {
    const RgbaSurface image = solid(2, 2, 255, 255, 255, 255);
    const json effects = {
        {"stroke", {{"size", 1.0}, {"inside", false}, {"opacity", 1.0}, {"red", 0.0}, {"green", 0.0}, {"blue", 0.0}}}};
    const RenderedEffects result = render_layer_effects(image, nullptr, effects);
    ASSERT_EQ(result.inset, 3);
    // The layer sits at (3,3); the ring covers the cell just outside it at (2,3).
    EXPECT_EQ(channel_at(result.image, 3, 3, 0), 255);
    EXPECT_LT(channel_at(result.image, 2, 3, 0), 20);
    EXPECT_GT(channel_at(result.image, 2, 3, 3), 200);
}

TEST(EffectsCompositeTest, ColorOverlayLayerRendersInTheDocument) {
    compositor::model::ProjectManifest manifest = comp_test::single_layer_manifest(10, 10);
    manifest.layers[0].effects =
        json{{"colorOverlay", {{"red", 1.0}, {"green", 0.0}, {"blue", 0.0}, {"opacity", 1.0}}}};
    const std::map<std::string, RgbaSurface> images{{comp_test::kLayerA, solid(10, 10, 255, 255, 255, 255)}};
    const RgbaSurface result = composite_document(manifest, images, {});
    // The whole opaque layer is overlaid red, at the document's own corner.
    EXPECT_EQ(channel_at(result, 0, 0, 0), 255);
    EXPECT_EQ(channel_at(result, 0, 0, 1), 0);
    EXPECT_EQ(channel_at(result, 0, 0, 2), 0);
    EXPECT_EQ(result.width(), 10);
}

TEST(EffectsTest, InnerGlowDarkensTheEdgesNotTheCenter) {
    const RgbaSurface image = solid(5, 5, 255, 255, 255, 255);
    const json effects = {
        {"innerGlow", {{"size", 2.0}, {"opacity", 1.0}, {"red", 1.0}, {"green", 0.0}, {"blue", 0.0}}}};
    const RenderedEffects result = render_layer_effects(image, nullptr, effects);
    // Center stays nearly white; an edge pixel picks up more of the red glow, so its green drops further.
    const int center = 2 + 2;  // inset 2, center of a 5x5
    const int center_green = channel_at(result.image, center, center, 1);
    const int edge_green = channel_at(result.image, 2, center, 1);
    EXPECT_GT(center_green, 240);
    EXPECT_LT(edge_green, center_green);
}

}  // namespace
