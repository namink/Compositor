#include "compositor/model/canvas_guide.hpp"

namespace compositor::model {

std::string_view to_string(CanvasGuide::Axis axis) {
    return axis == CanvasGuide::Axis::horizontal ? "horizontal" : "vertical";
}

bool guide_axis_from_string(std::string_view text, CanvasGuide::Axis& out) {
    if (text == "horizontal") {
        out = CanvasGuide::Axis::horizontal;
        return true;
    }
    if (text == "vertical") {
        out = CanvasGuide::Axis::vertical;
        return true;
    }
    return false;
}

}  // namespace compositor::model
