#include "layer_effects_dialog.hpp"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <functional>
#include <vector>

#include "tool_panel.hpp"

namespace compositor::appwin {
namespace {

using nlohmann::json;

[[nodiscard]] double num(const json& object, const char* key, double fallback) {
    return object.is_object() && object.contains(key) && object.at(key).is_number() ? object.at(key).get<double>()
                                                                                    : fallback;
}

[[nodiscard]] QColor color_of(const json& object, double red, double green, double blue) {
    return QColor::fromRgbF(num(object, "red", red), num(object, "green", green), num(object, "blue", blue));
}

struct Collector {
    std::function<void(json&)> write;
};

void add_effect_tab(QTabWidget* tabs, std::vector<Collector>& collectors, const QString& label, const char* key,
                    const json& current, QColor default_color, bool wants_angle_distance, bool wants_size) {
    const json effect = current.is_object() && current.contains(key) ? current.at(key) : json::object();
    auto* page = new QWidget(tabs);
    auto* form = new QFormLayout(page);

    auto* enabled = new QCheckBox(QStringLiteral("Enabled"), page);
    enabled->setChecked(effect.is_object() && effect.value("enabled", false));
    form->addRow(enabled);

    QColor color = color_of(effect, default_color.redF(), default_color.greenF(), default_color.blueF());
    auto* color_button = new QPushButton(page);
    color_button->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name()));
    QObject::connect(color_button, &QPushButton::clicked, page, [&color, color_button, page] {
        const QColor chosen = QColorDialog::getColor(color, page, QStringLiteral("Effect color"));
        if (chosen.isValid()) {
            color = chosen;
            color_button->setStyleSheet(QStringLiteral("background-color: %1").arg(color.name()));
        }
    });
    form->addRow(QStringLiteral("Color"), color_button);

    auto* opacity = new QDoubleSpinBox(page);
    opacity->setRange(0.0, 1.0);
    opacity->setDecimals(2);
    opacity->setValue(num(effect, "opacity", 1.0));
    form->addRow(QStringLiteral("Opacity"), opacity);

    QDoubleSpinBox* size = nullptr;
    if (wants_size) {
        size = new QDoubleSpinBox(page);
        size->setRange(0.0, 250.0);
        size->setValue(num(effect, "size", 4.0));
        form->addRow(QStringLiteral("Size"), size);
    }
    QDoubleSpinBox* distance = nullptr;
    QDoubleSpinBox* angle = nullptr;
    QDoubleSpinBox* blur = nullptr;
    if (wants_angle_distance) {
        distance = new QDoubleSpinBox(page);
        distance->setRange(0.0, 1000.0);
        distance->setValue(num(effect, "distance", 20.0));
        form->addRow(QStringLiteral("Distance"), distance);
        angle = new QDoubleSpinBox(page);
        angle->setRange(-180.0, 180.0);
        angle->setValue(num(effect, "angle", 90.0));
        form->addRow(QStringLiteral("Angle"), angle);
        blur = new QDoubleSpinBox(page);
        blur->setRange(0.0, 250.0);
        blur->setValue(num(effect, "blur", 20.0));
        form->addRow(QStringLiteral("Blur"), blur);
    }
    tabs->addTab(page, label);

    const std::string key_string = key;
    Collector collector;
    collector.write = [key_string, enabled, opacity, size, distance, angle, blur, &color](json& out) {
        json value = {{"enabled", enabled->isChecked()},
                      {"red", color.redF()},
                      {"green", color.greenF()},
                      {"blue", color.blueF()},
                      {"opacity", opacity->value()}};
        if (size != nullptr) {
            value["size"] = size->value();
        }
        if (distance != nullptr) {
            value["distance"] = distance->value();
            value["angle"] = angle->value();
            value["blur"] = blur->value();
        }
        out[key_string] = value;
    };
    collectors.push_back(std::move(collector));
}

}  // namespace

bool prompt_layer_effects(QWidget* parent, json& effects) {
    const json current = effects.is_object() ? effects : json::object();
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Layer Effects"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* tabs = new QTabWidget(&dialog);
    layout->addWidget(tabs);

    std::vector<Collector> collectors;
    add_effect_tab(tabs, collectors, QStringLiteral("Stroke"), "stroke", current, QColor(0, 0, 0), false, true);
    add_effect_tab(tabs, collectors, QStringLiteral("Drop Shadow"), "shadow", current, QColor(0, 0, 0), true, false);
    add_effect_tab(tabs, collectors, QStringLiteral("Outer Glow"), "outerGlow", current, QColor(255, 220, 80), false,
                   true);
    add_effect_tab(tabs, collectors, QStringLiteral("Inner Shadow"), "innerShadow", current, QColor(0, 0, 0), true,
                   false);
    add_effect_tab(tabs, collectors, QStringLiteral("Inner Glow"), "innerGlow", current, QColor(255, 255, 255), false,
                   true);
    add_effect_tab(tabs, collectors, QStringLiteral("Color Overlay"), "colorOverlay", current, QColor(220, 40, 40),
                   false, false);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    arrange_tool_panel(dialog, 440);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    json out = json::object();
    for (const Collector& collector : collectors) {
        collector.write(out);
    }
    effects = out;
    return true;
}

}  // namespace compositor::appwin
