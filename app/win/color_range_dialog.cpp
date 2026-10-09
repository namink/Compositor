#include "color_range_dialog.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "tool_panel.hpp"

namespace compositor::appwin {

bool prompt_color_range(QWidget* parent, QColor& color, int& fuzziness, bool& invert) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Color Range"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();

    auto* color_button = new QPushButton(&dialog);
    color_button->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name()));
    QObject::connect(color_button, &QPushButton::clicked, &dialog, [&] {
        const QColor chosen = QColorDialog::getColor(color, &dialog, QStringLiteral("Color Range"));
        if (chosen.isValid()) {
            color = chosen;
            color_button->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name()));
        }
    });
    form->addRow(QStringLiteral("Color"), color_button);

    auto* fuzziness_spin = new QDoubleSpinBox(&dialog);
    fuzziness_spin->setRange(0.0, 200.0);
    fuzziness_spin->setDecimals(0);
    fuzziness_spin->setValue(fuzziness);
    form->addRow(QStringLiteral("Fuzziness"), fuzziness_spin);
    layout->addLayout(form);

    auto* invert_check = new QCheckBox(QStringLiteral("Invert"), &dialog);
    invert_check->setChecked(invert);
    layout->addWidget(invert_check);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    arrange_tool_panel(dialog, 320);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    fuzziness = static_cast<int>(fuzziness_spin->value());
    invert = invert_check->isChecked();
    return true;
}

}  // namespace compositor::appwin
