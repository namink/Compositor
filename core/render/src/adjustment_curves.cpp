#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "adjustment_internal.hpp"

extern "C" {
#include "LevelsPixels.h"
}

#include "adjustment_json.hpp"

namespace compositor::render::detail {
namespace {

using nlohmann::json;

struct CurvePoint {
    double x = 0.0;
    double y = 0.0;
};

/// Shape-preserving cubic Hermite interpolation between handles, matching `CurvesSettings.value` in
/// the macOS app: it never overshoots between points, so a curve stays monotone where the handles do.
[[nodiscard]] double curve_value(const std::vector<CurvePoint>& points, double x) {
    if (points.size() < 2) {
        return x;
    }
    std::vector<double> slopes(points.size() - 1);
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        slopes[i] = (points[i + 1].y - points[i].y) / (points[i + 1].x - points[i].x);
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
        if (points[j].x <= x) {
            i = j;
        }
    }
    i = std::min(points.size() - 2, i);
    const double h = points[i + 1].x - points[i].x;
    const double t = std::clamp(h == 0.0 ? 0.0 : (x - points[i].x) / h, 0.0, 1.0);
    const double y = (2 * t * t * t - 3 * t * t + 1) * points[i].y + (t * t * t - 2 * t * t + t) * h * slope(i) +
                     (-2 * t * t * t + 3 * t * t) * points[i + 1].y + (t * t * t - t * t) * h * slope(i + 1);
    return std::clamp(y, 0.0, 255.0);
}

[[nodiscard]] std::vector<CurvePoint> read_channel(const json& list) {
    std::vector<CurvePoint> points;
    if (list.is_array()) {
        points.reserve(list.size());
        for (const json& item : list) {
            if (item.is_object()) {
                points.push_back(CurvePoint{number_or(item, "x", 0.0), number_or(item, "y", 0.0)});
            }
        }
    }
    return points;
}

}  // namespace

void apply_curves(const nlohmann::json& adjustment, RgbaSurface& surface) {
    const json& curves = child(adjustment, "curves");
    std::array<std::vector<CurvePoint>, 4> channels{};
    const json& lists = curves.contains("channels") ? curves.at("channels") : json::array();
    bool well_formed = lists.is_array() && lists.size() == 4;
    for (std::size_t c = 0; c < channels.size() && c < lists.size(); ++c) {
        channels[c] = read_channel(lists.at(c));
        if (channels[c].size() < 2) {
            well_formed = false;
        }
    }
    if (!well_formed) {
        return;
    }
    std::array<float, 768> table{};
    for (int out_channel = 1; out_channel <= 3; ++out_channel) {
        for (int i = 0; i <= 255; ++i) {
            const double after_channel = curve_value(channels[static_cast<std::size_t>(out_channel)], i);
            const double after_rgb = curve_value(channels[0], after_channel);
            table[static_cast<std::size_t>(out_channel - 1) * 256U + static_cast<std::size_t>(i)] =
                static_cast<float>(after_rgb / 255.0);
        }
    }
    const std::size_t count = static_cast<std::size_t>(surface.width()) * static_cast<std::size_t>(surface.height());
    levels_apply(surface.data(), count, table.data());
}

}  // namespace compositor::render::detail
