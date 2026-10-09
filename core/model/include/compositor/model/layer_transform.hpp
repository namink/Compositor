#pragma once
#include <string>
#include <string_view>

namespace compositor::model {

/// How a layer's pixels are resampled when scaled. Matches `LayerSampling` in the macOS app.
enum class LayerSampling {
    nearest,
    smooth,
    high,
};

/// The manifest spelling: "Nearest", "Smooth", "High quality".
[[nodiscard]] std::string_view to_string(LayerSampling sampling);
[[nodiscard]] bool sampling_from_string(std::string_view text, LayerSampling& out);

/// Unrotated bounds in document pixels; rotation is clockwise, in degrees, around the center.
///
/// The JSON is `{ "origin": [x, y], "size": [w, h], "rotation": r, "flipX": b, "flipY": b,
/// "sampling": "..." }`. `rotation`, the flips and `sampling` are optional on read and default to
/// upright / "High quality", matching the writing guide.
struct LayerTransform {
    double origin_x = 0.0;
    double origin_y = 0.0;
    double width = 0.0;
    double height = 0.0;
    double rotation = 0.0;
    bool flip_x = false;
    bool flip_y = false;
    LayerSampling sampling = LayerSampling::high;

    [[nodiscard]] double center_x() const { return origin_x + width / 2.0; }
    [[nodiscard]] double center_y() const { return origin_y + height / 2.0; }

    /// Finite origin/size/rotation, a positive size within the side limit, and an origin within the
    /// document coordinate range. Mirrors `LayerTransform.isValid`.
    [[nodiscard]] bool is_valid() const;
};

}  // namespace compositor::model
