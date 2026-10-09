#pragma once
#include <QWidget>
#include <nlohmann/json.hpp>

namespace compositor::appwin {

/// Edit a `LayerTextStyle` — the macOS app's editable-text metadata — in a dialog. `style` is updated
/// in place and true is returned on OK. Mirrors the fields the macOS Type tool exposes: content,
/// font, size, color, alignment, tracking and leading.
[[nodiscard]] bool prompt_text(QWidget* parent, const QString& title, nlohmann::json& style);

}  // namespace compositor::appwin
