#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "adjustment_internal.hpp"

extern "C" {
#include "LevelsPixels.h"
}

#include "adjustment_json.hpp"

namespace compositor::render::detail {
namespace {

using nlohmann::json;

enum RangeIndex { kMaster = 0, kReds, kYellows, kGreens, kCyans, kBlues, kMagentas, kRangeCount };

[[nodiscard]] bool range_from_string(const std::string& text, int& out) {
    static const std::array<const char*, kRangeCount> kNames{"Master", "Reds",  "Yellows", "Greens",
                                                             "Cyans",  "Blues", "Magentas"};
    for (int i = 0; i < kRangeCount; ++i) {
        if (text == kNames[static_cast<std::size_t>(i)]) {
            out = i;
            return true;
        }
    }
    return false;
}

struct HueBand {
    double falloff_start = 0.0;
    double range_start = 0.0;
    double range_end = 360.0;
    double falloff_end = 360.0;
};

/// The band Photoshop starts each range with.
[[nodiscard]] HueBand default_band(int range) {
    switch (range) {
    case kReds:
        return {315, 345, 15, 45};
    case kYellows:
        return {15, 45, 75, 105};
    case kGreens:
        return {75, 105, 135, 165};
    case kCyans:
        return {135, 165, 195, 225};
    case kBlues:
        return {195, 225, 255, 285};
    case kMagentas:
        return {255, 285, 315, 345};
    default:
        return {0, 0, 360, 360};
    }
}

[[nodiscard]] double wrap360(double value) {
    double remainder = std::fmod(value, 360.0);
    return remainder < 0.0 ? remainder + 360.0 : remainder;
}

[[nodiscard]] double forward(double from, double to) {
    double delta = std::fmod(to - from, 360.0);
    return delta < 0.0 ? delta + 360.0 : delta;
}

[[nodiscard]] double band_weight(const HueBand& band, double hue) {
    const double span = forward(band.falloff_start, band.falloff_end);
    if (span <= 0.0) {
        return 1.0;  // Master covers everything.
    }
    const double position = forward(band.falloff_start, hue);
    if (position > span) {
        return 0.0;
    }
    const double ramp_in = forward(band.falloff_start, band.range_start);
    const double plateau_end = forward(band.falloff_start, band.range_end);
    if (position < ramp_in) {
        return ramp_in > 0.0 ? position / ramp_in : 1.0;
    }
    if (position <= plateau_end) {
        return 1.0;
    }
    const double ramp_out = span - plateau_end;
    return ramp_out > 0.0 ? (span - position) / ramp_out : 1.0;
}

struct RangeAdjustment {
    double hue = 0.0;
    double saturation = 0.0;
    double lightness = 0.0;
    [[nodiscard]] bool is_identity() const { return hue == 0.0 && saturation == 0.0 && lightness == 0.0; }
};

struct HsvSettings {
    int range = kMaster;
    bool colorize = false;
    bool invert = false;
    std::array<RangeAdjustment, kRangeCount> adjustments{};
    std::array<HueBand, kRangeCount> bands{};

    HsvSettings() {
        for (int i = 0; i < kRangeCount; ++i) {
            bands[static_cast<std::size_t>(i)] = default_band(i);
        }
    }

    [[nodiscard]] double weight_of(int color_range, double hue) const {
        if (color_range == kMaster) {
            return 1.0;
        }
        const double weight = band_weight(bands[static_cast<std::size_t>(color_range)], hue);
        return invert && color_range == range ? 1.0 - weight : weight;
    }
};

struct Response {
    double shift = 0.0;
    double saturation = 0.0;
    double lightness = 0.0;
};

struct Rgb {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
};

void each_entry(const json& container, const std::function<void(const std::string&, const json&)>& visit) {
    if (container.is_object()) {
        for (auto it = container.begin(); it != container.end(); ++it) {
            visit(it.key(), it.value());
        }
    } else if (container.is_array()) {
        for (std::size_t i = 0; i + 1 < container.size(); i += 2) {
            if (container.at(i).is_string()) {
                visit(container.at(i).get<std::string>(), container.at(i + 1));
            }
        }
    }
}

[[nodiscard]] HsvSettings parse_settings(const json& adjustment) {
    HsvSettings settings;
    // The flat fields are the `resolvedHSV` fallback for projects saved before range settings existed.
    settings.adjustments[kMaster] =
        RangeAdjustment{number_or(adjustment, "hue", 0.0), number_or(adjustment, "saturation", 0.0),
                        number_or(adjustment, "lightness", 0.0)};
    settings.colorize = bool_or(adjustment, "colorize", false);
    if (!adjustment.is_object() || !adjustment.contains("hsvSettings") || !adjustment.at("hsvSettings").is_object()) {
        return settings;
    }
    const json hsv = adjustment.at("hsvSettings");
    settings.colorize = bool_or(hsv, "colorize", false);
    settings.invert = bool_or(hsv, "invertRange", false);
    int range = kMaster;
    if (range_from_string(string_or(hsv, "range", "Master"), range)) {
        settings.range = range;
    }
    settings.adjustments = {};
    if (hsv.contains("adjustments")) {
        each_entry(hsv.at("adjustments"), [&settings](const std::string& name, const json& value) {
            int index = kMaster;
            if (value.is_object() && range_from_string(name, index)) {
                settings.adjustments[static_cast<std::size_t>(index)] =
                    RangeAdjustment{number_or(value, "hue", 0.0), number_or(value, "saturation", 0.0),
                                    number_or(value, "lightness", 0.0)};
            }
        });
    }
    if (hsv.contains("bands")) {
        each_entry(hsv.at("bands"), [&settings](const std::string& name, const json& value) {
            int index = kMaster;
            if (value.is_object() && range_from_string(name, index)) {
                settings.bands[static_cast<std::size_t>(index)] =
                    HueBand{number_or(value, "falloffStart", 0.0), number_or(value, "rangeStart", 0.0),
                            number_or(value, "rangeEnd", 360.0), number_or(value, "falloffEnd", 360.0)};
            }
        });
    }
    return settings;
}

/// Photoshop's Saturation: below 0 scales toward gray, above 0 divides by what is left.
[[nodiscard]] double adjusted_saturation(double saturation, double amount) {
    amount = std::clamp(amount / 100.0, -1.0, 1.0);
    if (amount <= 0.0) {
        return std::max(0.0, saturation * (1.0 + amount));
    }
    return amount >= 1.0 ? (saturation > 0.0 ? 1.0 : 0.0) : std::min(1.0, saturation / (1.0 - amount));
}

[[nodiscard]] Rgb to_hsl(double red, double green, double blue) {
    const double high = std::max({red, green, blue});
    const double low = std::min({red, green, blue});
    const double lightness = (high + low) / 2.0;
    const double delta = high - low;
    if (delta <= 0.0) {
        return {0.0, 0.0, lightness};
    }
    const double saturation = std::min(1.0, delta / (1.0 - std::abs(2.0 * lightness - 1.0)));
    double hue;
    if (high == red) {
        hue = (green - blue) / delta;
    } else if (high == green) {
        hue = (blue - red) / delta + 2.0;
    } else {
        hue = (red - green) / delta + 4.0;
    }
    hue *= 60.0;
    if (hue < 0.0) {
        hue += 360.0;
    }
    return {hue, saturation, lightness};
}

[[nodiscard]] Rgb to_rgb(double hue, double saturation, double lightness) {
    if (saturation <= 0.0) {
        return {lightness, lightness, lightness};
    }
    const double chroma = (1.0 - std::abs(2.0 * lightness - 1.0)) * saturation;
    const double sector = hue / 60.0;
    const double second = chroma * (1.0 - std::abs(std::fmod(sector, 2.0) - 1.0));
    const double base = lightness - chroma / 2.0;
    Rgb result;
    switch (static_cast<int>(sector)) {
    case 0:
        result = {chroma, second, 0.0};
        break;
    case 1:
        result = {second, chroma, 0.0};
        break;
    case 2:
        result = {0.0, chroma, second};
        break;
    case 3:
        result = {0.0, second, chroma};
        break;
    case 4:
        result = {second, 0.0, chroma};
        break;
    default:
        result = {chroma, 0.0, second};
        break;
    }
    return {std::clamp(result.r + base, 0.0, 1.0), std::clamp(result.g + base, 0.0, 1.0),
            std::clamp(result.b + base, 0.0, 1.0)};
}

[[nodiscard]] std::vector<Response> hue_response(const HsvSettings& settings) {
    std::vector<Response> table(361);
    for (int degree = 0; degree <= 360; ++degree) {
        Response response;
        for (int range = 0; range < kRangeCount; ++range) {
            const RangeAdjustment& adjustment = settings.adjustments[static_cast<std::size_t>(range)];
            if (adjustment.is_identity()) {
                continue;
            }
            const double weight = settings.weight_of(range, degree);
            if (weight <= 0.0) {
                continue;
            }
            response.shift += adjustment.hue * weight;
            response.saturation += adjustment.saturation * weight;
            response.lightness += adjustment.lightness * weight;
        }
        table[static_cast<std::size_t>(degree)] = response;
    }
    return table;
}

[[nodiscard]] Rgb adjust(double red, double green, double blue, const HsvSettings& settings,
                         const std::vector<Response>& response) {
    Rgb hsl = to_hsl(red, green, blue);
    double lightness_amount = 0.0;
    if (settings.colorize) {
        const RangeAdjustment& selected = settings.adjustments[static_cast<std::size_t>(settings.range)];
        hsl.r = wrap360(selected.hue);
        hsl.g = std::clamp(selected.saturation / 100.0, 0.0, 1.0);
        lightness_amount = selected.lightness / 100.0;
    } else {
        const int degree = std::clamp(static_cast<int>(std::lround(hsl.r)), 0, 360);
        const Response& sampled = response[static_cast<std::size_t>(degree)];
        lightness_amount = sampled.lightness / 100.0;
        hsl.r = wrap360(hsl.r + sampled.shift);
        hsl.g = adjusted_saturation(hsl.g, sampled.saturation);
    }
    const double amount = std::clamp(lightness_amount, -1.0, 1.0);
    hsl.b = amount >= 0.0 ? hsl.b + (1.0 - hsl.b) * amount : hsl.b * (1.0 + amount);
    return to_rgb(hsl.r, hsl.g, std::clamp(hsl.b, 0.0, 1.0));
}

[[nodiscard]] std::vector<float> build_cube(const HsvSettings& settings) {
    constexpr int kDimension = 33;
    const std::vector<Response> response = hue_response(settings);
    std::vector<float> values(static_cast<std::size_t>(kDimension) * kDimension * kDimension * 4U);
    const double step = kDimension - 1;
    std::size_t index = 0;
    for (int blue = 0; blue < kDimension; ++blue) {
        for (int green = 0; green < kDimension; ++green) {
            for (int red = 0; red < kDimension; ++red) {
                const Rgb color = adjust(red / step, green / step, blue / step, settings, response);
                values[index] = static_cast<float>(color.r);
                values[index + 1] = static_cast<float>(color.g);
                values[index + 2] = static_cast<float>(color.b);
                values[index + 3] = 1.0F;
                index += 4;
            }
        }
    }
    return values;
}

}  // namespace

void apply_hsv(const nlohmann::json& adjustment, RgbaSurface& surface) {
    const HsvSettings settings = parse_settings(adjustment);
    const std::vector<float> cube = build_cube(settings);
    const std::size_t count = static_cast<std::size_t>(surface.width()) * static_cast<std::size_t>(surface.height());
    cube_apply(surface.data(), count, cube.data(), 33);
}

}  // namespace compositor::render::detail
