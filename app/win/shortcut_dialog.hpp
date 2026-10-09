#pragma once
#include <QString>
#include <QVector>

class QWidget;

namespace compositor::appwin {

/// One remappable command: its id, the label shown, and its current key sequence as text.
struct ShortcutEntry {
    QString id;
    QString label;
    QString sequence;
};

/// Show a dialog to edit the key sequences. Returns true and updates `entries` on OK, false on Cancel.
[[nodiscard]] bool edit_shortcuts(QWidget* parent, QVector<ShortcutEntry>& entries);

}  // namespace compositor::appwin
