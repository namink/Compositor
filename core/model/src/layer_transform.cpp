#include "compositor/model/layer_transform.hpp"

#include <cmath>

namespace compositor::model {

std::string_view to_string(LayerSampling sampling) {
    switch (sampling) {
    case LayerSampling::nearest:
        return "Nearest";
    case LayerSampling::smooth:
        return "Smooth";
    case LayerSampling::high:
        return "High quality";
    }
    return "High quality";
}

bool sampling_from_string(std::string_view text, LayerSampling& out) {
    if (text == "Nearest") {
        out = LayerSampling::nearest;
        return true;
    }
    if (text == "Smooth") {
        out = LayerSampling::smooth;
        return true;
    }
    if (text == "High quality") {
        out = LayerSampling::high;
        return true;
    }
    return false;
}

bool LayerTransform::is_valid() const {
    const bool finite = std::isfinite(origin_x) && std::isfinite(origin_y) && std::isfinite(width) &&
                        std::isfinite(height) && std::isfinite(rotation);
    const bool sized = width >= 1.0 && width <= 300'000.0 && height >= 1.0 && height <= 300'000.0;
    const bool placed = std::abs(origin_x) <= 1'000'000.0 && std::abs(origin_y) <= 1'000'000.0;
    return finite && sized && placed;
}

}  // namespace compositor::model
