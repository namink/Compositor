#include "compositor/render/adjustment.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include "AdjustPixels.h"
#include "DitherPixels.h"
#include "LevelsPixels.h"
#include "NoisePixels.h"
#include "ScanlinesPixels.h"
}

#include "adjustment_internal.hpp"
#include "adjustment_json.hpp"
#include "compositor/render/blur.hpp"

namespace compositor::render {
namespace {

using detail::bool_or;
using detail::child;
using detail::number_or;
using detail::string_or;
using nlohmann::json;

struct Color {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
};

[[nodiscard]] Color read_color(const json& object, Color fallback) {
    return Color{number_or(object, "red", fallback.r), number_or(object, "green", fallback.g),
                 number_or(object, "blue", fallback.b)};
}

// --- Levels / Exposure tables (3 x 256 floats, red then green then blue) ------------------------

struct LevelRange {
    double black = 0.0;
    double gamma = 1.0;
    double white = 255.0;
    double output_black = 0.0;
    double output_white = 255.0;
};

[[nodiscard]] LevelRange normalized(LevelRange range) {
    range.black = std::clamp(range.black, 0.0, 254.0);
    range.white = std::clamp(range.white, range.black + 1.0, 255.0);
    range.gamma = std::clamp(range.gamma, 0.1, 9.99);
    range.output_black = std::clamp(range.output_black, 0.0, 255.0);
    range.output_white = std::clamp(range.output_white, 0.0, 255.0);
    return range;
}

[[nodiscard]] int dither_style_from_string(const std::string& name) {
    if (name == "Floyd-Steinberg") {
        return DITHER_FLOYD_STEINBERG;
    }
    if (name == "Bayer 2") {
        return DITHER_BAYER_2;
    }
    if (name == "Bayer 4") {
        return DITHER_BAYER_4;
    }
    if (name == "Bayer 8") {
        return DITHER_BAYER_8;
    }
    if (name == "Dots") {
        return DITHER_DOTS;
    }
    if (name == "Lines") {
        return DITHER_LINES;
    }
    if (name == "Diamonds") {
        return DITHER_DIAMONDS;
    }
    if (name == "Patterns") {
        return DITHER_PATTERNS;
    }
    if (name == "Glyphs") {
        return DITHER_GLYPHS;
    }
    return DITHER_ATKINSON;
}

[[nodiscard]] double apply_range(LevelRange range, double value) {
    range = normalized(range);
    const double input = std::clamp((value * 255.0 - range.black) / (range.white - range.black), 0.0, 1.0);
    return (range.output_black + std::pow(input, 1.0 / range.gamma) * (range.output_white - range.output_black)) /
           255.0;
}

[[nodiscard]] LevelRange read_range(const json& object) {
    return LevelRange{number_or(object, "black", 0.0), number_or(object, "gamma", 1.0),
                      number_or(object, "white", 255.0), number_or(object, "outputBlack", 0.0),
                      number_or(object, "outputWhite", 255.0)};
}

[[nodiscard]] std::array<float, 768> levels_table(const json& adjustment) {
    std::array<LevelRange, 4> ranges{LevelRange{}, LevelRange{}, LevelRange{}, LevelRange{}};
    const json& levels = child(adjustment, "levels");
    if (levels.contains("ranges") && levels.at("ranges").is_array()) {
        const json& list = levels.at("ranges");
        for (std::size_t i = 0; i < ranges.size() && i < list.size(); ++i) {
            ranges[i] = read_range(list.at(i));
        }
    }
    std::array<float, 768> table{};
    for (int channel = 0; channel < 3; ++channel) {
        for (int i = 0; i <= 255; ++i) {
            const double value = static_cast<double>(i) / 255.0;
            table[static_cast<std::size_t>(channel) * 256U + static_cast<std::size_t>(i)] = static_cast<float>(
                apply_range(ranges[0], apply_range(ranges[static_cast<std::size_t>(channel) + 1U], value)));
        }
    }
    return table;
}

[[nodiscard]] std::array<float, 768> exposure_table(const json& adjustment) {
    const json& settings = child(adjustment, "exposureSettings");
    const double exposure = number_or(settings, "exposure", 0.0);
    const double offset = number_or(settings, "offset", 0.0);
    const double gamma = number_or(settings, "gamma", 1.0);
    const double scale = std::pow(2.0, exposure);
    std::array<float, 768> table{};
    for (int i = 0; i <= 255; ++i) {
        const double encoded = static_cast<double>(i) / 255.0;
        double linear = encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
        linear = std::pow(std::max(0.0, linear * scale + offset), 1.0 / gamma);
        const double output = linear <= 0.0031308 ? linear * 12.92 : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
        const float value = static_cast<float>(std::clamp(output, 0.0, 1.0));
        for (int channel = 0; channel < 3; ++channel) {
            table[static_cast<std::size_t>(channel) * 256U + static_cast<std::size_t>(i)] = value;
        }
    }
    return table;
}

[[nodiscard]] std::array<float, 768> invert_table() {
    std::array<float, 768> table{};
    for (int i = 0; i <= 255; ++i) {
        const float value = static_cast<float>(255 - i) / 255.0F;
        for (int channel = 0; channel < 3; ++channel) {
            table[static_cast<std::size_t>(channel) * 256U + static_cast<std::size_t>(i)] = value;
        }
    }
    return table;
}

// --- Gradient Map table (256 x 3 straight sRGB bytes) -------------------------------------------

[[nodiscard]] std::array<std::uint8_t, 768> gradient_table(const json& adjustment) {
    const json& settings = child(adjustment, "gradientMapSettings");
    const Color shadows = read_color(child(settings, "shadows"), Color{0.0, 0.0, 0.0});
    const Color highlights = read_color(child(settings, "highlights"), Color{1.0, 1.0, 1.0});
    const bool reversed = bool_or(settings, "reversed", false);
    const Color dark = reversed ? highlights : shadows;
    const Color light = reversed ? shadows : highlights;
    std::array<std::uint8_t, 768> table{};
    for (int i = 0; i <= 255; ++i) {
        const double t = static_cast<double>(i) / 255.0;
        const double channels[3] = {dark.r + (light.r - dark.r) * t, dark.g + (light.g - dark.g) * t,
                                    dark.b + (light.b - dark.b) * t};
        for (int c = 0; c < 3; ++c) {
            table[static_cast<std::size_t>(i) * 3U + static_cast<std::size_t>(c)] =
                static_cast<std::uint8_t>(std::lround(std::clamp(channels[c], 0.0, 1.0) * 255.0));
        }
    }
    return table;
}

// --- Blending the effect over the original by the layer's opacity --------------------------------

void blend_by_opacity(RgbaSurface& canvas, const RgbaSurface& adjusted, double opacity, const RgbaSurface* coverage) {
    std::uint8_t* base = canvas.data();
    const std::uint8_t* effect = adjusted.data();
    const std::size_t pixels = static_cast<std::size_t>(canvas.width()) * static_cast<std::size_t>(canvas.height());
    if (opacity >= 1.0 && coverage == nullptr) {
        std::copy(effect, effect + pixels * 4U, base);
        return;
    }
    for (std::size_t p = 0; p < pixels; ++p) {
        double factor = opacity;
        if (coverage != nullptr) {
            // The coverage map is flat gray, so its red channel is the per-pixel strength.
            factor *= static_cast<double>(coverage->data()[p * 4U]) / 255.0;
        }
        for (std::size_t c = 0; c < 4U; ++c) {
            const std::size_t at = p * 4U + c;
            base[at] = static_cast<std::uint8_t>(
                std::lround(static_cast<double>(base[at]) * (1.0 - factor) + static_cast<double>(effect[at]) * factor));
        }
    }
}

}  // namespace

std::array<double, 1024> histogram(const RgbaSurface& image) {
    std::array<double, 1024> bins{};
    if (image.empty()) {
        return bins;
    }
    levels_histogram(image.data(), nullptr,
                     static_cast<std::size_t>(image.width()) * static_cast<std::size_t>(image.height()), bins.data());
    return bins;
}

void apply_adjustment(const nlohmann::json& adjustment, RgbaSurface& canvas, double opacity,
                      const RgbaSurface* mask_coverage) {
    if (canvas.empty() || !adjustment.is_object() || opacity <= 0.0) {
        return;
    }
    const std::string kind = string_or(adjustment, "kind", "");
    const int width = canvas.width();
    const int height = canvas.height();
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const std::size_t stride = static_cast<std::size_t>(width) * 4U;

    RgbaSurface adjusted = canvas;
    std::uint8_t* pixels = adjusted.data();
    bool applied = true;

    if (kind == "Invert" || kind == "Levels" || kind == "Exposure") {
        std::array<float, 768> table = kind == "Levels"     ? levels_table(adjustment)
                                       : kind == "Exposure" ? exposure_table(adjustment)
                                                            : invert_table();
        levels_apply(pixels, count, table.data());
    } else if (kind == "Gradient Map") {
        const std::array<std::uint8_t, 768> table = gradient_table(adjustment);
        adjust_gradient_map(pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride,
                            table.data());
    } else if (kind == "Black & White") {
        const json& settings = child(adjustment, "blackWhiteSettings");
        const float weights[6] = {static_cast<float>(number_or(settings, "reds", 40.0) / 100.0),
                                  static_cast<float>(number_or(settings, "yellows", 60.0) / 100.0),
                                  static_cast<float>(number_or(settings, "greens", 40.0) / 100.0),
                                  static_cast<float>(number_or(settings, "cyans", 60.0) / 100.0),
                                  static_cast<float>(number_or(settings, "blues", 20.0) / 100.0),
                                  static_cast<float>(number_or(settings, "magentas", 80.0) / 100.0)};
        const bool tint = bool_or(settings, "tint", false);
        adjust_black_white(pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride, weights,
                           tint ? 1 : 0, number_or(settings, "tintHue", 40.0),
                           number_or(settings, "tintSaturation", 20.0) / 100.0);
    } else if (kind == "Color Balance") {
        const json& settings = child(adjustment, "colorBalanceSettings");
        const float shadows[3] = {static_cast<float>(number_or(settings, "shadowCyanRed", 0.0) / 100.0),
                                  static_cast<float>(number_or(settings, "shadowMagentaGreen", 0.0) / 100.0),
                                  static_cast<float>(number_or(settings, "shadowYellowBlue", 0.0) / 100.0)};
        const float midtones[3] = {static_cast<float>(number_or(settings, "midCyanRed", 0.0) / 100.0),
                                   static_cast<float>(number_or(settings, "midMagentaGreen", 0.0) / 100.0),
                                   static_cast<float>(number_or(settings, "midYellowBlue", 0.0) / 100.0)};
        const float highlights[3] = {static_cast<float>(number_or(settings, "highlightCyanRed", 0.0) / 100.0),
                                     static_cast<float>(number_or(settings, "highlightMagentaGreen", 0.0) / 100.0),
                                     static_cast<float>(number_or(settings, "highlightYellowBlue", 0.0) / 100.0)};
        adjust_color_balance(pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride, shadows,
                             midtones, highlights, bool_or(settings, "preserveLuminosity", true) ? 1 : 0);
    } else if (kind == "Grain") {
        const json& settings = child(adjustment, "grainSettings");
        adjust_grain(pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride,
                     number_or(settings, "amount", 25.0), number_or(settings, "size", 1.5),
                     number_or(settings, "roughness", 50.0),
                     static_cast<std::uint32_t>(number_or(settings, "seed", 0.0)), 0.0, 0.0, 1.0);
    } else if (kind == "Add Noise") {
        noise_add(pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride,
                  static_cast<float>(number_or(adjustment, "noiseAmount", 10.0)),
                  bool_or(adjustment, "noiseGaussian", false) ? 1 : 0,
                  bool_or(adjustment, "noiseMonochromatic", false) ? 1 : 0,
                  static_cast<std::uint32_t>(number_or(adjustment, "noiseSeed", 0.0)));
    } else if (kind == "Hue/Saturation") {
        detail::apply_hsv(adjustment, adjusted);
    } else if (kind == "Curves") {
        detail::apply_curves(adjustment, adjusted);
    } else if (kind == "Gaussian Blur") {
        adjusted = gaussian_blur(canvas, number_or(adjustment, "blurRadius", 10.0));
    } else if (kind == "Motion Blur") {
        adjusted = motion_blur(canvas, number_or(adjustment, "motionDistance", 10.0),
                               number_or(adjustment, "motionAngle", 0.0));
    } else if (kind == "Camera Raw") {
        const json& calibration = child(adjustment, "calibration");
        const bool has_calibration = calibration.is_object() && !calibration.empty();
        if (has_calibration) {
            adjust_camera_raw_calibration(
                pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride,
                number_or(calibration, "shadowTint", 0.0), number_or(calibration, "redHue", 0.0),
                number_or(calibration, "redSaturation", 0.0), number_or(calibration, "greenHue", 0.0),
                number_or(calibration, "greenSaturation", 0.0), number_or(calibration, "blueHue", 0.0),
                number_or(calibration, "blueSaturation", 0.0),
                static_cast<int>(number_or(calibration, "processVersion", 0.0)));
        }
        const double warm = number_or(adjustment, "temperature", 0.0) / 100.0;
        const double magenta = number_or(adjustment, "tint", 0.0) / 100.0;
        const double red_gain = 1.0 + 0.35 * warm + 0.15 * magenta;
        const double green_gain = 1.0 - 0.30 * magenta;
        const double blue_gain = 1.0 - 0.35 * warm + 0.15 * magenta;
        adjust_camera_raw(pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride, red_gain,
                          green_gain, blue_gain, number_or(adjustment, "exposure", 0.0),
                          number_or(adjustment, "contrast", 0.0), number_or(adjustment, "highlights", 0.0),
                          number_or(adjustment, "shadows", 0.0), number_or(adjustment, "whites", 0.0),
                          number_or(adjustment, "blacks", 0.0), number_or(adjustment, "vibrance", 0.0),
                          number_or(adjustment, "saturation", 0.0), 0);
        detail::apply_camera_raw_curve_color(adjustment, pixels, width, height, stride);
        const double texture = number_or(adjustment, "texture", 0.0);
        const double clarity = number_or(adjustment, "clarity", 0.0);
        const double dehaze = number_or(adjustment, "dehaze", 0.0);
        const double glow = number_or(adjustment, "glow", 0.0);
        const double vignette = number_or(adjustment, "vignetteAmount", 0.0);
        if (texture != 0.0 || clarity != 0.0 || dehaze != 0.0 || glow != 0.0 || vignette != 0.0) {
            adjust_camera_raw_effects(
                pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride, texture, clarity,
                dehaze, glow, static_cast<int>(number_or(adjustment, "glowStyle", 0.0)),
                number_or(adjustment, "glowRange", 0.0), number_or(adjustment, "glowSpread", 0.0),
                number_or(adjustment, "glowWarmth", 0.0), vignette, number_or(adjustment, "vignetteMidpoint", 50.0),
                number_or(adjustment, "vignetteRoundness", 0.0), number_or(adjustment, "vignetteFeather", 50.0),
                number_or(adjustment, "vignetteHighlights", 0.0),
                static_cast<int>(number_or(adjustment, "vignetteStyle", 0.0)), 1.0);
        }
        const double grain_amount = number_or(adjustment, "grainAmount", 0.0);
        if (grain_amount > 0.0) {
            const double grain_size = 0.5 + (number_or(adjustment, "grainSize", 25.0) / 100.0) * 19.5;
            adjust_grain(pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride,
                         grain_amount, grain_size, number_or(adjustment, "grainRoughness", 50.0),
                         static_cast<std::uint32_t>(number_or(adjustment, "seed", 0.0)), 0.0, 0.0, 1.0);
        }
        const json& detail = child(adjustment, "detail");
        if (detail.is_object() && !detail.empty()) {
            adjust_camera_raw_detail(
                pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride,
                number_or(detail, "sharpenAmount", 0.0), number_or(detail, "sharpenRadius", 1.0),
                number_or(detail, "sharpenDetail", 25.0), number_or(detail, "sharpenMasking", 0.0),
                number_or(detail, "noiseLuminance", 0.0), number_or(detail, "noiseLuminanceDetail", 50.0),
                number_or(detail, "noiseLuminanceContrast", 0.0), number_or(detail, "noiseColor", 0.0),
                number_or(detail, "noiseColorDetail", 50.0), number_or(detail, "noiseColorSmoothness", 50.0), 1.0);
        }
        const json& optics = child(adjustment, "optics");
        if (optics.is_object() && !optics.empty()) {
            adjust_camera_raw_optics(
                pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride,
                bool_or(optics, "removeChromatic", false) ? 1 : 0,
                static_cast<int>(number_or(optics, "lensProfile", 0.0)), number_or(optics, "profileDistortion", 0.0),
                number_or(optics, "profileVignetting", 0.0), number_or(optics, "distortionK", 0.0),
                number_or(optics, "purpleAmount", 0.0), number_or(optics, "purpleHueLow", 0.0),
                number_or(optics, "purpleHueHigh", 0.0), number_or(optics, "greenAmount", 0.0),
                number_or(optics, "greenHueLow", 0.0), number_or(optics, "greenHueHigh", 0.0),
                number_or(optics, "vignetteAmount", 0.0), number_or(optics, "vignetteMidpoint", 50.0), 1.0);
        }
    } else if (kind == "Dither") {
        DitherParams params;
        std::memset(&params, 0, sizeof(params));
        params.style = dither_style_from_string(string_or(adjustment, "style", "Atkinson"));
        params.levels = static_cast<int>(number_or(adjustment, "levels", 2.0));
        params.diffusion = static_cast<float>(number_or(adjustment, "diffusion", 1.0));
        params.density = static_cast<float>(number_or(adjustment, "density", 0.0));
        params.contrast = static_cast<float>(number_or(adjustment, "contrast", 0.0));
        params.cell = static_cast<int>(number_or(adjustment, "cell", 6.0));
        params.angle = static_cast<float>(number_or(adjustment, "angleDegrees", 45.0) * 3.14159265358979323846 / 180.0);
        params.lightOnDark = bool_or(adjustment, "lightOnDark", false) ? 1 : 0;
        params.originalColors = bool_or(adjustment, "originalColors", false) ? 1 : 0;
        params.dark[0] = static_cast<std::uint8_t>(number_or(child(adjustment, "dark"), "red", 0.0));
        params.dark[1] = static_cast<std::uint8_t>(number_or(child(adjustment, "dark"), "green", 0.0));
        params.dark[2] = static_cast<std::uint8_t>(number_or(child(adjustment, "dark"), "blue", 0.0));
        params.light[0] = static_cast<std::uint8_t>(number_or(child(adjustment, "light"), "red", 255.0));
        params.light[1] = static_cast<std::uint8_t>(number_or(child(adjustment, "light"), "green", 255.0));
        params.light[2] = static_cast<std::uint8_t>(number_or(child(adjustment, "light"), "blue", 255.0));
        dither_apply(adjusted.data(), static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride,
                     &params);
    } else if (kind == "Scanlines") {
        ScanlinesParams params;
        std::memset(&params, 0, sizeof(params));
        const double spacing = number_or(adjustment, "lineSpacing", 4.0);
        params.spacing = static_cast<int>(std::lround(spacing));
        params.thickness = static_cast<float>(number_or(adjustment, "thickness", 70.0) / 100.0);
        params.dots = static_cast<float>(number_or(adjustment, "dots", 0.0) / 100.0);
        params.wobble = static_cast<float>(number_or(adjustment, "wobble", 0.0));
        params.displace = static_cast<float>(number_or(adjustment, "displace", 0.0));
        params.threshold = static_cast<float>(number_or(adjustment, "threshold", 0.0) / 100.0);
        params.split = static_cast<float>(std::lround(number_or(adjustment, "split", 0.0)));
        params.density = static_cast<float>(number_or(adjustment, "density", 0.0) / 100.0);
        params.contrast = static_cast<float>(number_or(adjustment, "contrast", 0.0) / 100.0);
        params.blackLevel = static_cast<float>(number_or(adjustment, "blackLevel", 0.0) / 100.0);
        params.smoothness = static_cast<float>(number_or(adjustment, "smoothness", 50.0) / 100.0);
        params.originalColors = bool_or(adjustment, "originalColors", false) ? 1 : 0;
        const Color dark = read_color(child(adjustment, "dark"), Color{0.0, 0.0, 0.0});
        const Color light = read_color(child(adjustment, "light"), Color{1.0, 1.0, 1.0});
        params.dark[0] = static_cast<std::uint8_t>(std::lround(dark.r * 255.0));
        params.dark[1] = static_cast<std::uint8_t>(std::lround(dark.g * 255.0));
        params.dark[2] = static_cast<std::uint8_t>(std::lround(dark.b * 255.0));
        params.light[0] = static_cast<std::uint8_t>(std::lround(light.r * 255.0));
        params.light[1] = static_cast<std::uint8_t>(std::lround(light.g * 255.0));
        params.light[2] = static_cast<std::uint8_t>(std::lround(light.b * 255.0));
        scanlines_apply(pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride, &params);
        const double glow = number_or(adjustment, "glow", 0.0);
        if (glow > 0.0) {
            // The glow is a wide, soft bloom of the lines; blurred at the full sigma for now (the macOS app blurs
            // a shrunk copy, which is the same light for a fraction of the work).
            const double sigma = spacing * 3.0 + 3.0;
            const RgbaSurface bloom =
                gaussian_blur(adjusted, sigma);
            dither_glow(pixels, bloom.data(), static_cast<std::size_t>(width), static_cast<std::size_t>(height),
                        stride, static_cast<float>(glow / 100.0 * 2.5));
        }
    } else {
        applied = false;
    }

    if (applied) {
        blend_by_opacity(canvas, adjusted, opacity, mask_coverage);
    }
}

}  // namespace compositor::render
