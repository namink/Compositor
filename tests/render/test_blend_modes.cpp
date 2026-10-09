#include <gtest/gtest.h>

#include <cmath>

#include "compositor/render/blend_mode_math.hpp"

namespace {

using compositor::model::LayerBlendMode;
using compositor::render::blend_channel;
using compositor::render::blend_color;
using compositor::render::RgbF;

float channel(LayerBlendMode mode, float cb, float cs) {
    return blend_channel(mode, cb, cs);
}

TEST(BlendChannelTest, StandardSeparableValues) {
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::normal, 0.2F, 0.7F), 0.7F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::multiply, 0.5F, 0.5F), 0.25F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::multiply, 1.0F, 0.4F), 0.4F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::screen, 0.5F, 0.5F), 0.75F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::darken, 0.3F, 0.8F), 0.3F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::lighten, 0.3F, 0.8F), 0.8F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::difference, 0.8F, 0.3F), 0.5F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::exclusion, 0.5F, 0.5F), 0.5F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::linear_dodge, 0.6F, 0.6F), 1.0F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::linear_burn, 0.6F, 0.6F), 0.2F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::subtract, 0.6F, 0.2F), 0.4F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::subtract, 0.2F, 0.6F), 0.0F);
}

TEST(BlendChannelTest, DodgeBurnAndScreenEndpoints) {
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::color_dodge, 0.0F, 0.5F), 0.0F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::color_dodge, 0.5F, 1.0F), 1.0F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::color_dodge, 0.5F, 0.5F), 1.0F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::color_burn, 1.0F, 0.5F), 1.0F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::color_burn, 0.5F, 0.0F), 0.0F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::color_burn, 0.75F, 0.5F), 0.5F);
}

TEST(BlendChannelTest, OverlayIsHardLightWithRolesSwapped) {
    const float cb = 0.25F;
    const float cs = 0.75F;
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::overlay, cb, cs), channel(LayerBlendMode::hard_light, cs, cb));
    // Backdrop below the midpoint darkens, above it lightens.
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::overlay, 0.25F, 0.5F), 0.25F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::overlay, 0.75F, 0.5F), 0.75F);
}

TEST(BlendChannelTest, SoftLightMatchesTheReferenceCurve) {
    // 50% grey leaves the backdrop alone.
    EXPECT_NEAR(channel(LayerBlendMode::soft_light, 0.4F, 0.5F), 0.4F, 1e-6);
    // A light source lightens; the exact value follows the W3C curve the macOS app uses Core Image for.
    const float lighten = channel(LayerBlendMode::soft_light, 0.5F, 0.8F);
    EXPECT_GT(lighten, 0.5F);
    const float darken = channel(LayerBlendMode::soft_light, 0.5F, 0.2F);
    EXPECT_LT(darken, 0.5F);
}

TEST(BlendChannelTest, VividLinearPinAndHardMix) {
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::vivid_light, 0.5F, 0.25F), channel(LayerBlendMode::color_burn, 0.5F, 0.5F));
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::vivid_light, 0.5F, 0.75F),
                    channel(LayerBlendMode::color_dodge, 0.5F, 0.5F));
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::linear_light, 0.5F, 0.25F), 0.0F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::linear_light, 0.5F, 0.75F), 1.0F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::pin_light, 0.8F, 0.25F), 0.5F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::pin_light, 0.8F, 0.75F), 0.8F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::hard_mix, 0.5F, 0.25F), 0.0F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::hard_mix, 0.5F, 0.75F), 1.0F);
}

TEST(BlendChannelTest, Divide) {
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::divide, 0.5F, 0.5F), 1.0F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::divide, 0.25F, 0.5F), 0.5F);
    EXPECT_FLOAT_EQ(channel(LayerBlendMode::divide, 0.5F, 0.0F), 1.0F);  // divide by nothing is white
}

float luminance(const RgbF& c) {
    return 0.3F * c.r + 0.59F * c.g + 0.11F * c.b;
}

TEST(BlendColorTest, NonSeparableModesPreserveLuminosityOrSaturation) {
    const RgbF cb{0.2F, 0.5F, 0.8F};
    const RgbF cs{0.9F, 0.1F, 0.3F};

    const RgbF luminosity = blend_color(LayerBlendMode::luminosity, cb, cs);
    EXPECT_NEAR(luminance(luminosity), luminance(cs), 1e-4);

    const RgbF color = blend_color(LayerBlendMode::color, cb, cs);
    EXPECT_NEAR(luminance(color), luminance(cb), 1e-4);

    const RgbF hue = blend_color(LayerBlendMode::hue, cb, cs);
    EXPECT_NEAR(luminance(hue), luminance(cb), 1e-4);

    const RgbF saturation = blend_color(LayerBlendMode::saturation, cb, cs);
    EXPECT_NEAR(luminance(saturation), luminance(cb), 1e-4);
}

TEST(BlendColorTest, SeparableModesRunPerChannel) {
    const RgbF cb{0.2F, 0.4F, 0.6F};
    const RgbF cs{0.5F, 0.5F, 0.5F};
    const RgbF result = blend_color(LayerBlendMode::multiply, cb, cs);
    EXPECT_FLOAT_EQ(result.r, 0.1F);
    EXPECT_FLOAT_EQ(result.g, 0.2F);
    EXPECT_FLOAT_EQ(result.b, 0.3F);
}

}  // namespace
