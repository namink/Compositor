#pragma once
#include <QIcon>
#include <QString>

namespace compositor::appwin {

/// A simple painted icon for a tool, drawn to read like the macOS app's tool strip. No image assets
/// are involved, so there is nothing to ship or scale.
[[nodiscard]] QIcon tool_icon(const QString& tool);

/// A painted icon for a panel button (plus, folder, mask, effects, adjustment, trash, layers), drawn in
/// the same line style to stand in for the macOS app's SF Symbols.
[[nodiscard]] QIcon panel_icon(const QString& name);

}  // namespace compositor::appwin
