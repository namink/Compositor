#include <QAction>
#include <QKeySequence>
#include <QSettings>

#include "main_window.hpp"
#include "shortcut_dialog.hpp"

// Remappable keyboard shortcuts, persisted with QSettings.

namespace compositor::appwin {

void MainWindow::register_action(const QString& id, QAction* action) {
    shortcut_actions_.insert(id, action);
}

void MainWindow::load_shortcuts() {
    QSettings settings;
    for (auto it = shortcut_actions_.begin(); it != shortcut_actions_.end(); ++it) {
        const QString key = QStringLiteral("shortcuts/") + it.key();
        if (settings.contains(key)) {
            it.value()->setShortcut(QKeySequence(settings.value(key).toString()));
        }
    }
}

void MainWindow::show_shortcuts_dialog() {
    QVector<ShortcutEntry> entries;
    entries.reserve(shortcut_actions_.size());
    for (auto it = shortcut_actions_.begin(); it != shortcut_actions_.end(); ++it) {
        QString label = it.value()->text();
        label.remove(QLatin1Char('&'));
        entries.push_back(ShortcutEntry{it.key(), label, it.value()->shortcut().toString()});
    }
    if (!edit_shortcuts(this, entries)) {
        return;
    }
    QSettings settings;
    for (const ShortcutEntry& entry : entries) {
        QAction* action = shortcut_actions_.value(entry.id);
        if (action != nullptr) {
            action->setShortcut(QKeySequence(entry.sequence));
            settings.setValue(QStringLiteral("shortcuts/") + entry.id, entry.sequence);
        }
    }
}

}  // namespace compositor::appwin
