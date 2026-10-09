#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "compositor/render/adjustment.hpp"

// Automatic Levels, ported from the macOS app's `LevelsAutomatic.swift`: a histogram of the layer's
// own pixels, endpoints at 0.1% clipping, and ranges that preserve channel relationships (Contrast)
// or adjust each channel (Color), optionally neutralizing the midtones.

namespace compositor::render {
namespace {

using nlohmann::json;

constexpr std::array<double, 256> kEmptyBins{};

[[nodiscard]] bool endpoints(const std::array<double, 256>& bins, double& low, double& high) {
    double total = 0.0;
    for (double value : bins) {
        total += value;
    }
    if (total <= 0.0) {
        return false;
    }
    double sum = 0.0;
    int low_index = 0;
    for (int i = 0; i < 256; ++i) {
        sum += bins[static_cast<std::size_t>(i)];
        if (sum > total * 0.001) {
            low_index = i;
            break;
        }
    }
    sum = 0.0;
    int high_index = 255;
    for (int i = 255; i >= 0; --i) {
        sum += bins[static_cast<std::size_t>(i)];
        if (sum > total * 0.001) {
            high_index = i;
            break;
        }
    }
    if (low_index < high_index) {
        low = static_cast<double>(low_index);
        high = static_cast<double>(high_index);
        return true;
    }
    return false;
}

[[nodiscard]] json default_range() {
    return json{{"black", 0.0}, {"gamma", 1.0}, {"white", 255.0}, {"outputBlack", 0.0}, {"outputWhite", 255.0}};
}

[[nodiscard]] json range_json(double black, double white, double gamma) {
    return json{{"black", black}, {"gamma", gamma}, {"white", white}, {"outputBlack", 0.0}, {"outputWhite", 255.0}};
}

[[nodiscard]] double apply_range(double black, double white, double value) {
    return std::clamp((value * 255.0 - black) / (white - black), 0.0, 1.0);
}

}  // namespace

nlohmann::json auto_levels_settings(const RgbaSurface& layer, int mode) {
    std::array<std::array<double, 256>, 3> bins{};
    for (int y = 0; y < layer.height(); ++y) {
        for (int x = 0; x < layer.width(); ++x) {
            const std::uint8_t* pixel = layer.data() + layer.offset(x, y);
            const double alpha = pixel[3];
            if (alpha <= 0.0) {
                continue;
            }
            const double channels[3] = {pixel[0] / alpha, pixel[1] / alpha, pixel[2] / alpha};
            for (int c = 0; c < 3; ++c) {
                const int index = static_cast<int>(std::lround(std::clamp(channels[c], 0.0, 1.0) * 255.0));
                bins[static_cast<std::size_t>(c)][static_cast<std::size_t>(index)] += 1.0;
            }
        }
    }

    json ranges = json::array();
    if (mode == 0) {
        // Contrast: a shared interval across the channels preserves channel relationships.
        double low = std::numeric_limits<double>::infinity();
        double high = -std::numeric_limits<double>::infinity();
        for (int c = 0; c < 3; ++c) {
            double channel_low = 0.0;
            double channel_high = 0.0;
            if (endpoints(bins[static_cast<std::size_t>(c)], channel_low, channel_high)) {
                low = std::min(low, channel_low);
                high = std::max(high, channel_high);
            }
        }
        ranges.push_back(low < high ? range_json(low, high, 1.0) : default_range());
        for (int c = 0; c < 3; ++c) {
            ranges.push_back(default_range());
        }
        return json{{"ranges", ranges}};
    }

    ranges.push_back(default_range());
    for (int c = 0; c < 3; ++c) {
        double low = 0.0;
        double high = 0.0;
        if (!endpoints(bins[static_cast<std::size_t>(c)], low, high)) {
            ranges.push_back(default_range());
            continue;
        }
        double gamma = 1.0;
        if (mode == 2) {
            double total = 0.0;
            double weighted = 0.0;
            for (int i = 0; i < 256; ++i) {
                const double count = bins[static_cast<std::size_t>(c)][static_cast<std::size_t>(i)];
                total += count;
                weighted += apply_range(low, high, static_cast<double>(i) / 255.0) * count;
            }
            const double mean = total > 0.0 ? weighted / total : 0.0;
            if (mean > 0.0 && mean < 1.0) {
                gamma = std::clamp(std::log(mean) / std::log(0.5), 0.1, 9.99);
            }
        }
        ranges.push_back(range_json(low, high, gamma));
    }
    return json{{"ranges", ranges}};
}

}  // namespace compositor::render
