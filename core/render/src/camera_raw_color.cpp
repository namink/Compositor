#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "adjustment_internal.hpp"
#include "adjustment_json.hpp"

extern "C" {
#include "AdjustPixels.h"
}

// Camera Raw Curve / Mixer / Grading, split out so `adjustment.cpp` stays within the soft size limit.
// Ports `CameraRawCurveSettings`, `CameraRawMixerSettings` and `CameraRawGradingSettings` from the
// macOS app: the parametric + point tone curves, the eight-family HSL mixer, the point colors and the
// four grading wheels, handed to `adjust_camera_raw_curve_color`.

namespace compositor::render::detail {
namespace {

using nlohmann::json;
using CurvePt = std::pair<double, double>;

[[nodiscard]] double monotone_curve(const std::vector<CurvePt>& points, double x) {
    if (points.size() < 2) {
        return x;
    }
    std::vector<double> slopes(points.size() - 1);
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const double dx = points[i + 1].first - points[i].first;
        slopes[i] = dx == 0.0 ? 0.0 : (points[i + 1].second - points[i].second) / dx;
    }
    const auto slope = [&](std::size_t j) {
        if (j == 0) {
            return slopes.front();
        }
        if (j == points.size() - 1) {
            return slopes.back();
        }
        if (slopes[j - 1] * slopes[j] <= 0.0) {
            return 0.0;
        }
        return 2.0 / (1.0 / slopes[j - 1] + 1.0 / slopes[j]);
    };
    std::size_t i = 0;
    for (std::size_t j = 0; j + 1 < points.size(); ++j) {
        if (points[j].first <= x) {
            i = j;
        }
    }
    i = std::min(points.size() - 2, i);
    const double h = points[i + 1].first - points[i].first;
    const double t = std::clamp(h == 0.0 ? 0.0 : (x - points[i].first) / h, 0.0, 1.0);
    return std::clamp((2 * t * t * t - 3 * t * t + 1) * points[i].second + (t * t * t - 2 * t * t + t) * h * slope(i) +
                          (-2 * t * t * t + 3 * t * t) * points[i + 1].second + (t * t * t - t * t) * h * slope(i + 1),
                      0.0, 1.0);
}

[[nodiscard]] const json& curve_list(const json& object, const char* key) {
    static const json kEmpty = json::array();
    if (object.is_object() && object.contains(key) && object.at(key).is_array()) {
        return object.at(key);
    }
    return kEmpty;
}

[[nodiscard]] std::vector<CurvePt> read_curve_points(const json& list) {
    std::vector<CurvePt> points;
    if (list.is_array()) {
        for (const json& item : list) {
            if (item.is_object()) {
                points.emplace_back(number_or(item, "x", 0.0), number_or(item, "y", 0.0));
            }
        }
    }
    if (points.size() < 2) {
        points = {{0.0, 0.0}, {1.0, 1.0}};
    }
    return points;
}

[[nodiscard]] double bend(double tone, double lower, double low, double upper, double high) {
    const double strength = 1.66;
    if (tone < lower && lower > 0.0) {
        return lower * std::pow(tone / lower, std::pow(2.0, -low / 100.0 * strength));
    }
    if (tone > upper && upper < 1.0) {
        const double rest = 1.0 - upper;
        return 1.0 - rest * std::pow((1.0 - tone) / rest, std::pow(2.0, high / 100.0 * strength));
    }
    return tone;
}

[[nodiscard]] std::array<float, 256> tone_table(const json& curve) {
    const double shadows = number_or(curve, "shadows", 0.0);
    const double darks = number_or(curve, "darks", 0.0);
    const double lights = number_or(curve, "lights", 0.0);
    const double highlights = number_or(curve, "highlights", 0.0);
    const double shadow_split = number_or(curve, "shadowSplit", 25.0) / 100.0;
    const double dark_split = number_or(curve, "darkSplit", 50.0) / 100.0;
    const double light_split = number_or(curve, "lightSplit", 75.0) / 100.0;
    const std::vector<CurvePt> rgb = read_curve_points(curve_list(curve, "rgb"));
    const bool parametric = shadows != 0.0 || darks != 0.0 || lights != 0.0 || highlights != 0.0;
    std::vector<CurvePt> anchors;
    if (parametric) {
        for (int k = 0; k <= 32; ++k) {
            const double x = static_cast<double>(k) / 32.0;
            const double bent =
                bend(bend(x, shadow_split, shadows, light_split, highlights), dark_split, darks, dark_split, lights);
            anchors.emplace_back(x, bent);
        }
    }
    std::array<float, 256> table{};
    for (int i = 0; i <= 255; ++i) {
        const double tone = static_cast<double>(i) / 255.0;
        const double value = parametric ? monotone_curve(anchors, tone) : tone;
        table[static_cast<std::size_t>(i)] = static_cast<float>(monotone_curve(rgb, value));
    }
    return table;
}

[[nodiscard]] std::array<float, 256> channel_table(const json& curve, const char* key) {
    const std::vector<CurvePt> points = read_curve_points(curve_list(curve, key));
    std::array<float, 256> table{};
    for (int i = 0; i <= 255; ++i) {
        table[static_cast<std::size_t>(i)] = static_cast<float>(monotone_curve(points, static_cast<double>(i) / 255.0));
    }
    return table;
}

}  // namespace

void apply_camera_raw_curve_color(const nlohmann::json& adjustment, std::uint8_t* pixels, int width, int height,
                                  std::size_t stride) {
    const json& curve = child(adjustment, "curve");
    const json& mixer = child(adjustment, "mixer");
    const json& grading = child(adjustment, "grading");
    if (!curve.is_object() && !mixer.is_object() && !grading.is_object()) {
        return;
    }
    const std::array<float, 256> tone = tone_table(curve);
    const std::array<float, 256> red = channel_table(curve, "red");
    const std::array<float, 256> green = channel_table(curve, "green");
    const std::array<float, 256> blue = channel_table(curve, "blue");
    std::array<float, 24> mixer_floats{};
    const auto component = [](const json& list, int index) {
        return list.is_array() && static_cast<std::size_t>(index) < list.size() && list.at(index).is_number()
                   ? list.at(index).get<double>() / 100.0
                   : 0.0;
    };
    const json& hues = curve_list(mixer, "hue");
    const json& sats = curve_list(mixer, "saturation");
    const json& lums = curve_list(mixer, "luminance");
    for (int i = 0; i < 8; ++i) {
        mixer_floats[static_cast<std::size_t>(i)] = static_cast<float>(component(hues, i));
        mixer_floats[8U + static_cast<std::size_t>(i)] = static_cast<float>(component(sats, i));
        mixer_floats[16U + static_cast<std::size_t>(i)] = static_cast<float>(component(lums, i));
    }
    std::vector<float> point_floats;
    const json& points = curve_list(mixer, "points");
    if (points.is_array()) {
        for (const json& point : points) {
            if (!point.is_object()) {
                continue;
            }
            point_floats.push_back(static_cast<float>(number_or(point, "hue", 0.0) / 360.0));
            point_floats.push_back(static_cast<float>(number_or(point, "saturation", 0.0)));
            point_floats.push_back(static_cast<float>(number_or(point, "luminance", 0.0)));
            point_floats.push_back(static_cast<float>(number_or(point, "hueShift", 0.0) / 100.0));
            point_floats.push_back(static_cast<float>(number_or(point, "saturationShift", 0.0) / 100.0));
            point_floats.push_back(static_cast<float>(number_or(point, "luminanceShift", 0.0) / 100.0));
            point_floats.push_back(static_cast<float>(number_or(point, "hueRange", 30.0) / 360.0));
            point_floats.push_back(static_cast<float>(number_or(point, "saturationRange", 0.4)));
            point_floats.push_back(static_cast<float>(number_or(point, "luminanceRange", 0.4)));
        }
    }
    std::array<float, 12> grade_floats{};
    const char* const wheels[4] = {"shadows", "midtones", "highlights", "global"};
    for (int wheel = 0; wheel < 4; ++wheel) {
        const json& wheel_json = child(grading, wheels[wheel]);
        grade_floats[static_cast<std::size_t>(wheel) * 3U] =
            static_cast<float>(number_or(wheel_json, "hue", 0.0) / 360.0);
        grade_floats[static_cast<std::size_t>(wheel) * 3U + 1U] =
            static_cast<float>(number_or(wheel_json, "saturation", 0.0) / 100.0);
        grade_floats[static_cast<std::size_t>(wheel) * 3U + 2U] =
            static_cast<float>(number_or(wheel_json, "luminance", 0.0) / 100.0);
    }
    adjust_camera_raw_curve_color(
        pixels, static_cast<std::size_t>(width), static_cast<std::size_t>(height), stride, tone.data(), red.data(),
        green.data(), blue.data(), number_or(curve, "refineSaturation", 0.0) / 100.0, mixer_floats.data(),
        static_cast<int>(point_floats.size() / 9), point_floats.empty() ? nullptr : point_floats.data(),
        grade_floats.data(), number_or(grading, "blending", 50.0) / 100.0, number_or(grading, "balance", 0.0) / 100.0,
        -1);
}

}  // namespace compositor::render::detail
