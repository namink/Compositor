#include "text_runs_dialog.hpp"

#include <QColor>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <string>

#include "tool_panel.hpp"

namespace compositor::appwin {
namespace {

using nlohmann::json;

[[nodiscard]] int run_end(const json& run) {
    return run.value("location", 0) + run.value("length", 0);
}

/// Drops any run overlapping [start, start+length), then inserts `run` and keeps the list sorted: runs
/// must be sorted and not overlap (see `docs/project-format.md`).
void apply_run(json& runs, int start, int length, json run) {
    json kept = json::array();
    for (const json& existing : runs) {
        const int location = existing.value("location", 0);
        const int end = run_end(existing);
        if (location < start + length && end > start) {
            continue;  // overlaps the new range
        }
        kept.push_back(existing);
    }
    kept.push_back(std::move(run));
    std::sort(kept.begin(), kept.end(),
              [](const json& a, const json& b) { return a.value("location", 0) < b.value("location", 0); });
    runs = std::move(kept);
}

}  // namespace

bool prompt_text_runs(QWidget* parent, json& style) {
    if (!style.is_object()) {
        style = json::object();
    }
    const QString content = QString::fromStdString(style.value("content", std::string()));
    const int content_length = content.size();
    if (content_length <= 0) {
        return false;
    }
    json color_runs =
        style.contains("colorRuns") && style.at("colorRuns").is_array() ? style.at("colorRuns") : json::array();
    json font_runs =
        style.contains("fontRuns") && style.at("fontRuns").is_array() ? style.at("fontRuns") : json::array();

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Text Runs"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    layout->addLayout(form);

    auto* start = new QDoubleSpinBox(&dialog);
    start->setRange(0.0, content_length - 1.0);
    start->setDecimals(0);
    form->addRow(QStringLiteral("Start (chars)"), start);

    auto* length = new QDoubleSpinBox(&dialog);
    length->setRange(1.0, static_cast<double>(content_length));
    length->setDecimals(0);
    length->setValue(1.0);
    form->addRow(QStringLiteral("Length"), length);

    QColor color(style.value("red", 0.0) * 255, style.value("green", 0.0) * 255, style.value("blue", 0.0) * 255);
    auto* color_button = new QPushButton(&dialog);
    color_button->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name()));
    QObject::connect(color_button, &QPushButton::clicked, &dialog, [&color, color_button, &dialog] {
        const QColor chosen = QColorDialog::getColor(color, &dialog, QStringLiteral("Run color"));
        if (chosen.isValid()) {
            color = chosen;
            color_button->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name()));
        }
    });
    form->addRow(QStringLiteral("Color"), color_button);

    auto* font = new QFontComboBox(&dialog);
    font->setCurrentFont(QFont(QString::fromStdString(style.value("fontName", std::string("Helvetica")))));
    form->addRow(QStringLiteral("Font"), font);

    auto* status = new QLabel(&dialog);
    const auto update_status = [&] {
        status->setText(QStringLiteral("Color runs: %1   Font runs: %2")
                            .arg(static_cast<int>(color_runs.size()))
                            .arg(static_cast<int>(font_runs.size())));
    };
    update_status();
    layout->addWidget(status);

    auto* apply_color = new QPushButton(QStringLiteral("Apply Color to Range"), &dialog);
    QObject::connect(apply_color, &QPushButton::clicked, &dialog, [&] {
        const int at = static_cast<int>(start->value());
        const int count = std::min(static_cast<int>(length->value()), content_length - at);
        apply_run(color_runs, at, count,
                  json{{"location", at},
                       {"length", count},
                       {"red", color.redF()},
                       {"green", color.greenF()},
                       {"blue", color.blueF()}});
        update_status();
    });
    layout->addWidget(apply_color);

    auto* apply_font = new QPushButton(QStringLiteral("Apply Font to Range"), &dialog);
    QObject::connect(apply_font, &QPushButton::clicked, &dialog, [&] {
        const int at = static_cast<int>(start->value());
        const int count = std::min(static_cast<int>(length->value()), content_length - at);
        apply_run(font_runs, at, count,
                  json{{"location", at}, {"length", count}, {"fontName", font->currentFont().family().toStdString()}});
        update_status();
    });
    layout->addWidget(apply_font);

    auto* clear = new QPushButton(QStringLiteral("Clear All Runs"), &dialog);
    QObject::connect(clear, &QPushButton::clicked, &dialog, [&] {
        color_runs = json::array();
        font_runs = json::array();
        update_status();
    });
    layout->addWidget(clear);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    arrange_tool_panel(dialog, 360);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    if (color_runs.empty()) {
        style.erase("colorRuns");
    } else {
        style["colorRuns"] = color_runs;
    }
    if (font_runs.empty()) {
        style.erase("fontRuns");
    } else {
        style["fontRuns"] = font_runs;
    }
    return true;
}

}  // namespace compositor::appwin
