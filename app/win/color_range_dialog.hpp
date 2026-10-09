#pragma once
#include <QColor>
class QWidget;

namespace compositor::appwin {

/// Select > Color Range options: a target color, a fuzziness (0–200) and whether to invert. All are
/// left as they were on Cancel.
[[nodiscard]] bool prompt_color_range(QWidget* parent, QColor& color, int& fuzziness, bool& invert);

}  // namespace compositor::appwin
