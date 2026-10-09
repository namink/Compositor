#include "shortcut_dialog.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace compositor::appwin {

bool edit_shortcuts(QWidget* parent, QVector<ShortcutEntry>& entries) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Keyboard Shortcuts"));
    dialog.resize(420, 560);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QStringLiteral("Click a field, press the keys, then OK."), &dialog));

    auto* scroll = new QScrollArea(&dialog);
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget(scroll);
    auto* form = new QVBoxLayout(inner);
    QVector<QKeySequenceEdit*> editors;
    for (ShortcutEntry& entry : entries) {
        auto* row = new QWidget(inner);
        auto* rowLayout = new QVBoxLayout(row);
        rowLayout->setContentsMargins(0, 2, 0, 2);
        auto* label = new QLabel(entry.label, row);
        auto* editor = new QKeySequenceEdit(QKeySequence(entry.sequence), row);
        rowLayout->addWidget(label);
        rowLayout->addWidget(editor);
        form->addWidget(row);
        editors.push_back(editor);
    }
    scroll->setWidget(inner);
    layout->addWidget(scroll, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    for (int i = 0; i < entries.size(); ++i) {
        entries[i].sequence = editors[i]->keySequence().toString();
    }
    return true;
}

}  // namespace compositor::appwin
