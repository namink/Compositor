#pragma once
#include <QDialog>
#include <QPoint>
#include <QWidget>

namespace compositor::appwin {

/// Arrange a tool dialog the way the macOS app shows its floating tool panels: a fixed width along the
/// document window's right edge, near the top. Qt keeps these modal, so this only positions and sizes
/// them; it is a close stand-in for the docked 440-wide `NSPanel`s.
inline void arrange_tool_panel(QDialog& dialog, int width = 380) {
    dialog.setMinimumWidth(width);
    dialog.adjustSize();
    if (QWidget* parent = dialog.parentWidget()) {
        const QPoint top_right = parent->mapToGlobal(QPoint(parent->width(), 0));
        int x = top_right.x() - width - 12;
        if (x < 0) {
            x = 0;
        }
        dialog.move(x, top_right.y() + 8);
    }
}

}  // namespace compositor::appwin
