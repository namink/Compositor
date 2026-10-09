#include "text_dialog.hpp"

#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "tool_panel.hpp"

namespace compositor::appwin {

bool prompt_text(QWidget* parent, const QString& title, nlohmann::json& style) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();

    auto* content = new QPlainTextEdit(QString::fromStdString(style.value("content", std::string("Text"))), &dialog);
    content->setMinimumHeight(90);
    form->addRow(QStringLiteral("Content"), content);

    auto* font = new QFontComboBox(&dialog);
    font->setCurrentFont(QFont(QString::fromStdString(style.value("fontName", std::string("Helvetica")))));
    form->addRow(QStringLiteral("Font"), font);

    auto* size = new QDoubleSpinBox(&dialog);
    size->setRange(1.0, 2000.0);
    size->setDecimals(0);
    size->setValue(style.value("fontSize", 72.0));
    form->addRow(QStringLiteral("Size"), size);

    QColor color = QColor::fromRgbF(style.value("red", 0.0), style.value("green", 0.0), style.value("blue", 0.0));
    auto* color_button = new QPushButton(&dialog);
    color_button->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name()));
    QObject::connect(color_button, &QPushButton::clicked, &dialog, [&] {
        const QColor chosen = QColorDialog::getColor(color, &dialog, QStringLiteral("Text color"));
        if (chosen.isValid()) {
            color = chosen;
            color_button->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name()));
        }
    });
    form->addRow(QStringLiteral("Color"), color_button);

    auto* alignment = new QComboBox(&dialog);
    alignment->addItems({QStringLiteral("Left"), QStringLiteral("Center"), QStringLiteral("Right")});
    alignment->setCurrentText(QString::fromStdString(style.value("alignment", std::string("Left"))));
    form->addRow(QStringLiteral("Alignment"), alignment);

    auto* tracking = new QDoubleSpinBox(&dialog);
    tracking->setRange(-100.0, 1000.0);
    tracking->setValue(style.value("tracking", 0.0));
    form->addRow(QStringLiteral("Tracking"), tracking);

    auto* leading = new QDoubleSpinBox(&dialog);
    leading->setRange(0.0, 5000.0);
    leading->setValue(style.value("leading", 0.0));
    form->addRow(QStringLiteral("Leading (0 = auto)"), leading);

    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    arrange_tool_panel(dialog, 380);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    style["content"] = content->toPlainText().toStdString();
    style["fontName"] = font->currentFont().family().toStdString();
    style["fontSize"] = size->value();
    style["red"] = color.redF();
    style["green"] = color.greenF();
    style["blue"] = color.blueF();
    style["alignment"] = alignment->currentText().toStdString();
    style["tracking"] = tracking->value();
    style["leading"] = leading->value();
    return true;
}

}  // namespace compositor::appwin
