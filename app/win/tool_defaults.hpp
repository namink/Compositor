#pragma once
#include <QString>

namespace compositor::appwin {

/// Person-level tool options that keep their value across tabs and launches, the way Photoshop's tool
/// options and the macOS app's `ToolDefaults` do. Backed by QSettings under the `tool/` prefix.
[[nodiscard]] bool tool_bool(const QString& key, bool fallback);
[[nodiscard]] int tool_int(const QString& key, int fallback);
[[nodiscard]] double tool_double(const QString& key, double fallback);
[[nodiscard]] QString tool_string(const QString& key, const QString& fallback);

void set_tool_bool(const QString& key, bool value);
void set_tool_int(const QString& key, int value);
void set_tool_double(const QString& key, double value);
void set_tool_string(const QString& key, const QString& value);

}  // namespace compositor::appwin
