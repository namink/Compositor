#pragma once
#include <string>

namespace compositor::model {

/// A user-placed alignment line. A horizontal guide sits at a document Y; a vertical one at a
/// document X. Matches `CanvasGuide` in the macOS app.
struct CanvasGuide {
    enum class Axis {
        horizontal,
        vertical,
    };

    std::string id;
    Axis axis = Axis::horizontal;
    /// Document pixels: Y for a horizontal guide, X for a vertical one.
    double position = 0.0;
};

[[nodiscard]] std::string_view to_string(CanvasGuide::Axis axis);
[[nodiscard]] bool guide_axis_from_string(std::string_view text, CanvasGuide::Axis& out);

}  // namespace compositor::model
