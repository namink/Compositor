#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <string>

#include "compositor/model/manifest.hpp"
#include "compositor/render/adjustment.hpp"
#include "compositor/render/compositor.hpp"
#include "compositor/render/render_order.hpp"
#include "support/fixtures.hpp"

namespace {

using namespace compositor::model;
using namespace compositor::render;
using comp_test::kLayerA;
using comp_test::kLayerB;
using nlohmann::json;

RgbaSurface single_pixel(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    RgbaSurface surface(1, 1);
    surface.set(0, 0, r, g, b, a);
    return surface;
}

void expect_pixel(const RgbaSurface& surface, int r, int g, int b, int a, int tolerance = 1) {
    const std::uint8_t* p = surface.data();
    EXPECT_NEAR(p[0], r, tolerance);
    EXPECT_NEAR(p[1], g, tolerance);
    EXPECT_NEAR(p[2], b, tolerance);
    EXPECT_NEAR(p[3], a, tolerance);
}

ProjectLayerRecord adjustment_layer(const std::string& id, const std::string& name, json adjustment) {
    ProjectLayerRecord record;
    record.id = id;
    record.name = name;
    record.is_visible = true;
    record.transform = comp_test::full_canvas_transform(1, 1);
    record.adjustment = std::move(adjustment);
    return record;
}

TEST(AdjustmentTest, InvertFlipsEachChannel) {
    RgbaSurface canvas = single_pixel(60, 30, 0, 255);
    apply_adjustment(json{{"kind", "Invert"}}, canvas, 1.0);
    expect_pixel(canvas, 195, 225, 255, 255);
}

TEST(AdjustmentTest, InvertAtHalfOpacityIsHalfway) {
    RgbaSurface canvas = single_pixel(0, 0, 0, 255);
    apply_adjustment(json{{"kind", "Invert"}}, canvas, 0.5);
    expect_pixel(canvas, 128, 128, 128, 255, 2);
}

TEST(AdjustmentTest, LevelsMapsTheBlackAndWhitePoints) {
    // Black point 64, white point 128: 128 saturates to white, 64 goes to black.
    const json levels = {
        {"kind", "Levels"},
        {"levels",
         {{"ranges",
           json::array(
               {json{{"black", 64}, {"gamma", 1}, {"white", 128}, {"outputBlack", 0}, {"outputWhite", 255}},
                json{{"black", 64}, {"gamma", 1}, {"white", 128}, {"outputBlack", 0}, {"outputWhite", 255}},
                json{{"black", 64}, {"gamma", 1}, {"white", 128}, {"outputBlack", 0}, {"outputWhite", 255}},
                json{{"black", 64}, {"gamma", 1}, {"white", 128}, {"outputBlack", 0}, {"outputWhite", 255}}})}}}};
    RgbaSurface canvas = single_pixel(128, 128, 128, 255);
    apply_adjustment(levels, canvas, 1.0);
    expect_pixel(canvas, 255, 255, 255, 255);
}

TEST(AdjustmentTest, ExposureBrightensByStops) {
    RgbaSurface canvas = single_pixel(128, 128, 128, 255);
    apply_adjustment(json{{"kind", "Exposure"}, {"exposureSettings", {{"exposure", 1.0}}}}, canvas, 1.0);
    // One stop of light maps mid grey to about 176.
    expect_pixel(canvas, 176, 176, 176, 255, 2);
}

TEST(AdjustmentTest, GradientMapPicksBetweenShadowsAndHighlights) {
    const json gradient = {{"kind", "Gradient Map"},
                           {"gradientMapSettings",
                            {{"shadows", {{"red", 1.0}, {"green", 0.0}, {"blue", 0.0}}},
                             {"highlights", {{"red", 0.0}, {"green", 0.0}, {"blue", 1.0}}}}}};
    RgbaSurface black = single_pixel(0, 0, 0, 255);
    apply_adjustment(gradient, black, 1.0);
    expect_pixel(black, 255, 0, 0, 255);

    RgbaSurface mid = single_pixel(128, 128, 128, 255);
    apply_adjustment(gradient, mid, 1.0);
    expect_pixel(mid, 127, 0, 128, 255, 2);
}

TEST(AdjustmentTest, BlackAndWhiteWeighsPureRedAtItsDefault) {
    RgbaSurface canvas = single_pixel(255, 0, 0, 255);
    apply_adjustment(json{{"kind", "Black & White"}}, canvas, 1.0);
    // Photoshop's red weight is 40%.
    expect_pixel(canvas, 102, 102, 102, 255, 2);
}

TEST(AdjustmentTest, ColorBalanceAtZeroLeavesTheCanvasAlone) {
    RgbaSurface canvas = single_pixel(30, 90, 200, 255);
    apply_adjustment(json{{"kind", "Color Balance"}}, canvas, 1.0);
    expect_pixel(canvas, 30, 90, 200, 255);
}

TEST(AdjustmentTest, GrainAtZeroAmountIsANoOp) {
    RgbaSurface canvas = single_pixel(70, 80, 90, 200);
    apply_adjustment(json{{"kind", "Grain"}, {"grainSettings", {{"amount", 0.0}}}}, canvas, 1.0);
    expect_pixel(canvas, 70, 80, 90, 200);
}

TEST(AdjustmentTest, AddNoiseIsDeterministicForASeed) {
    const json noise = {{"kind", "Add Noise"}, {"noiseAmount", 60.0}, {"noiseSeed", 12345}};
    RgbaSurface first = single_pixel(128, 128, 128, 255);
    RgbaSurface second = single_pixel(128, 128, 128, 255);
    apply_adjustment(noise, first, 1.0);
    apply_adjustment(noise, second, 1.0);
    EXPECT_EQ(first.data()[0], second.data()[0]);
    EXPECT_EQ(first.data()[3], 255);  // alpha untouched
}

TEST(AdjustmentTest, HueSaturationShiftsHue) {
    RgbaSurface canvas = single_pixel(255, 0, 0, 255);
    apply_adjustment(json{{"kind", "Hue/Saturation"}, {"hue", 120.0}}, canvas, 1.0);
    expect_pixel(canvas, 0, 255, 0, 255, 2);
}

TEST(AdjustmentTest, HueSaturationSaturationEndpoints) {
    RgbaSurface red = single_pixel(255, 0, 0, 255);
    apply_adjustment(json{{"kind", "Hue/Saturation"}, {"saturation", -100.0}}, red, 1.0);
    expect_pixel(red, 128, 128, 128, 255, 2);

    RgbaSurface red2 = single_pixel(255, 0, 0, 255);
    apply_adjustment(json{{"kind", "Hue/Saturation"}, {"lightness", 100.0}}, red2, 1.0);
    expect_pixel(red2, 255, 255, 255, 255, 2);
}

TEST(AdjustmentTest, HueSaturationColorizeSetsAnAbsoluteHue) {
    RgbaSurface gray = single_pixel(128, 128, 128, 255);
    apply_adjustment(json{{"kind", "Hue/Saturation"}, {"colorize", true}, {"hue", 120.0}, {"saturation", 100.0}}, gray,
                     1.0);
    expect_pixel(gray, 0, 255, 0, 255, 3);
}

TEST(AdjustmentTest, HueSaturationIdentityLeavesPixelsAlone) {
    RgbaSurface canvas = single_pixel(12, 240, 77, 255);
    apply_adjustment(json{{"kind", "Hue/Saturation"}, {"hue", 0.0}}, canvas, 1.0);
    expect_pixel(canvas, 12, 240, 77, 255);
}

namespace {

json identity_curve() {
    return json::array({json{{"x", 0}, {"y", 0}}, json{{"x", 255}, {"y", 255}}});
}

json curves_adjustment(json rgb) {
    return json{
        {"kind", "Curves"},
        {"curves",
         {{"channel", "RGB"}, {"channels", json::array({rgb, identity_curve(), identity_curve(), identity_curve()})}}}};
}

}  // namespace

TEST(AdjustmentTest, CurvesIdentityLeavesPixelsAlone) {
    RgbaSurface canvas = single_pixel(10, 20, 30, 255);
    apply_adjustment(curves_adjustment(identity_curve()), canvas, 1.0);
    expect_pixel(canvas, 10, 20, 30, 255);
}

TEST(AdjustmentTest, CurvesInvertThroughTheRgbChannel) {
    const json inverted = json::array({json{{"x", 0}, {"y", 255}}, json{{"x", 255}, {"y", 0}}});
    RgbaSurface canvas = single_pixel(100, 100, 100, 255);
    apply_adjustment(curves_adjustment(inverted), canvas, 1.0);
    expect_pixel(canvas, 155, 155, 155, 255, 2);
}

TEST(AdjustmentTest, CurvesPerChannelBehavesAsATable) {
    const json red_channel = json::array({json{{"x", 0}, {"y", 0}}, json{{"x", 255}, {"y", 128}}});
    RgbaSurface canvas = single_pixel(200, 200, 200, 255);
    const json curves = json{
        {"kind", "Curves"},
        {"curves", {{"channels", json::array({identity_curve(), red_channel, identity_curve(), identity_curve()})}}}};
    apply_adjustment(curves, canvas, 1.0);
    // Red is pulled down to about 100; green and blue are untouched.
    expect_pixel(canvas, 100, 200, 200, 255, 2);
}

TEST(AdjustmentTest, CompositorAppliesAnAdjustmentLayerToTheCanvasBelow) {
    ProjectManifest manifest = comp_test::single_layer_manifest(1, 1);
    manifest.layers.push_back(adjustment_layer(kLayerB, "Invert", json{{"kind", "Invert"}}));
    const std::map<std::string, RgbaSurface> images{{kLayerA, single_pixel(60, 30, 0, 255)}};
    const RgbaSurface result = composite_document(manifest, images, {});
    expect_pixel(result, 195, 225, 255, 255);
}

TEST(AdjustmentTest, AdjustmentMaskLimitsWhereTheEffectApplies) {
    ProjectManifest manifest = comp_test::single_layer_manifest(2, 1);
    ProjectLayerRecord adjustment = adjustment_layer(kLayerB, "Invert", json{{"kind", "Invert"}});
    adjustment.transform = comp_test::full_canvas_transform(2, 1);
    adjustment.mask_file = std::string(kLayerB) + ".mask.png";
    adjustment.mask_enabled = true;
    manifest.layers.push_back(adjustment);

    RgbaSurface image(2, 1);
    image.set(0, 0, 100, 100, 100, 255);
    image.set(1, 0, 100, 100, 100, 255);
    RgbaSurface mask(2, 1);
    mask.set(0, 0, 0, 0, 0, 255);  // hidden: the effect does not apply here
    mask.set(1, 0, 255, 255, 255, 255);

    const std::map<std::string, RgbaSurface> images{{kLayerA, image}};
    const std::map<std::string, RgbaSurface> masks{{kLayerB, mask}};
    const RgbaSurface result = composite_document(manifest, images, masks);
    // Left pixel keeps its original value; the right is inverted to 155.
    const std::uint8_t* pixels = result.data();
    EXPECT_NEAR(pixels[0], 100, 1);
    EXPECT_NEAR(pixels[4], 155, 1);
}

TEST(RenderOrderTest, ListsAdjustmentLayersInOrder) {
    ProjectManifest manifest = comp_test::single_layer_manifest(1, 1);
    manifest.layers.push_back(adjustment_layer(kLayerB, "Invert", json{{"kind", "Invert"}}));
    const std::vector<DrawItem> items = resolve_draw_items(manifest);
    ASSERT_EQ(items.size(), 2U);
    EXPECT_FALSE(items[0].is_adjustment);
    EXPECT_TRUE(items[1].is_adjustment);
    EXPECT_EQ(items[1].layer->id, kLayerB);
}

TEST(AdjustmentTest, CameraRawSubmodulesRunAndKeepSize) {
    RgbaSurface canvas(16, 16);
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            canvas.set(x, y, static_cast<std::uint8_t>(x * 12), 120, static_cast<std::uint8_t>(y * 12), 255);
        }
    }
    const json adjustment = {{"kind", "Camera Raw"},
                             {"detail", {{"sharpenAmount", 30.0}, {"sharpenRadius", 1.0}}},
                             {"optics", {{"vignetteAmount", -25.0}, {"purpleAmount", 10.0}}},
                             {"calibration", {{"shadowTint", 6.0}, {"blueSaturation", 8.0}}},
                             {"curve", {{"darks", 20.0}, {"highlights", -10.0}, {"refineSaturation", 10.0}}},
                             {"mixer",
                              {{"hue", nlohmann::json::array({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0})},
                               {"saturation", nlohmann::json::array({10.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0})}}},
                             {"grading",
                              {{"shadows", {{"hue", 30.0}, {"saturation", 20.0}, {"luminance", -10.0}}},
                               {"blending", 50.0},
                               {"balance", 10.0}}}};
    apply_adjustment(adjustment, canvas, 1.0, nullptr);
    EXPECT_EQ(canvas.width(), 16);
    EXPECT_EQ(canvas.height(), 16);
}

}  // namespace
