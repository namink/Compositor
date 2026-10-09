#pragma once
#include <QWidget>
#include <nlohmann/json.hpp>

namespace compositor::appwin {

/// Edit a text layer's per-letter color and font runs (macOS's `colorRuns`/`fontRuns`): pick a
/// character range (UTF-16 offsets), a color or a face, and apply. `style` is the text metadata,
/// updated in place; false on Cancel.
[[nodiscard]] bool prompt_text_runs(QWidget* parent, nlohmann::json& style);

}  // namespace compositor::appwin
