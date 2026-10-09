#include "shape_edit_dialog.hpp"

#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "tool_panel.hpp"

namespace compositor::appwin {

bool prompt_shape(QWidget* parent, render::ShapeStyle& style) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Edit Shape"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();

    auto* kind = new QComboBox(&dialog);
    kind->addItems({QStringLiteral("Rectangle"), QStringLiteral("Ellipse"), QStringLiteral("Line")});
    kind->setCurrentIndex(style.kind == render::ShapeKind::ellipse ? 1 : style.kind == render::ShapeKind::line ? 2 : 0);
    form->addRow(QStringLiteral("Kind"), kind);

    QColor color = QColor::fromRgbF(style.red, style.green, style.blue);
    auto* color_button = new QPushButton(&dialog);
    color_button->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name()));
    QObject::connect(color_button, &QPushButton::clicked, &dialog, [&] {
        const QColor chosen = QColorDialog::getColor(color, &dialog, QStringLiteral("Shape color"));
        if (chosen.isValid()) {
            color = chosen;
            color_button->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name()));
        }
    });
    form->addRow(QStringLiteral("Color"), color_button);

    auto* corner = new QDoubleSpinBox(&dialog);
    corner->setRange(0.0, 2000.0);
    corner->setValue(style.corner_radius);
    form->addRow(QStringLiteral("Corner radius"), corner);

    auto* line_width = new QDoubleSpinBox(&dialog);
    line_width->setRange(1.0, 250.0);
    line_width->setValue(style.line_width > 0.0 ? style.line_width : 4.0);
    form->addRow(QStringLiteral("Line width"), line_width);

    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    arrange_tool_panel(dialog, 300);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    style.kind = kind->currentIndex() == 1   ? render::ShapeKind::ellipse
                 : kind->currentIndex() == 2 ? render::ShapeKind::line
                                             : render::ShapeKind::rectangle;
    style.red = color.redF();
    style.green = color.greenF();
    style.blue = color.blueF();
    style.corner_radius = corner->value();
    if (style.kind == render::ShapeKind::line) {
        style.line_width = line_width->value();
    }
    return true;
}

}  // namespace compositor::appwin
