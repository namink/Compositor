#pragma once
#include <QWidget>

#include "compositor/render/shape.hpp"

namespace compositor::appwin {

/// Edit an existing shape layer's style (macOS keeps a shape editable): kind, color, corner radius and
/// line width. `style` is updated in place; false on Cancel.
[[nodiscard]] bool prompt_shape(QWidget* parent, render::ShapeStyle& style);

}  // namespace compositor::appwin
