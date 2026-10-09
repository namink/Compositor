#include "adjustment_dialog.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <map>
#include <string>

#include "tool_panel.hpp"

namespace compositor::appwin {
namespace {

using nlohmann::json;

struct Field {
    const char* label;
    const char* key;
    double minimum;
    double maximum;
    int decimals;
};

[[nodiscard]] double jnum(const json& object, const char* key, double fallback) {
    if (object.is_object() && object.contains(key) && object.at(key).is_number()) {
        return object.at(key).get<double>();
    }
    return fallback;
}

[[nodiscard]] const json& jchild(const json& object, const char* key) {
    static const json kEmpty = json::object();
    if (object.is_object() && object.contains(key) && object.at(key).is_object()) {
        return object.at(key);
    }
    return kEmpty;
}

[[nodiscard]] std::vector<Field> fields_for(const QString& kind, const json& value) {
    if (kind == QStringLiteral("Hue/Saturation")) {
        return {{"Hue", "hue", -180, 180, 0},
                {"Saturation", "saturation", -100, 100, 0},
                {"Lightness", "lightness", -100, 100, 0}};
    }
    if (kind == QStringLiteral("Levels")) {
        return {{"Black", "black", 0, 254, 0}, {"Gamma", "gamma", 0.1, 9.99, 2}, {"White", "white", 1, 255, 0}};
    }
    if (kind == QStringLiteral("Exposure")) {
        return {{"Exposure", "exposure", -20, 20, 2},
                {"Offset", "offset", -0.5, 0.5, 3},
                {"Gamma", "gamma", 0.01, 9.99, 2}};
    }
    if (kind == QStringLiteral("Grain")) {
        return {{"Amount", "amount", 0, 100, 0},
                {"Size", "size", 0.5, 20, 2},
                {"Roughness", "roughness", 0, 100, 0},
                {"Seed", "seed", 0, 4294967295.0, 0}};
    }
    if (kind == QStringLiteral("Gaussian Blur")) {
        return {{"Radius", "blurRadius", 0.1, 250, 2}};
    }
    if (kind == QStringLiteral("Motion Blur")) {
        return {{"Distance", "motionDistance", 1, 2000, 0}, {"Angle", "motionAngle", -90, 90, 0}};
    }
    if (kind == QStringLiteral("Add Noise")) {
        return {{"Amount", "noiseAmount", 0.1, 400, 1}};
    }
    if (kind == QStringLiteral("Camera Raw")) {
        return {{"Temperature", "temperature", -100, 100, 0},
                {"Tint", "tint", -100, 100, 0},
                {"Exposure", "exposure", -5, 5, 2},
                {"Contrast", "contrast", -100, 100, 0},
                {"Highlights", "highlights", -100, 100, 0},
                {"Shadows", "shadows", -100, 100, 0},
                {"Whites", "whites", -100, 100, 0},
                {"Blacks", "blacks", -100, 100, 0},
                {"Vibrance", "vibrance", -100, 100, 0},
                {"Saturation", "saturation", -100, 100, 0},
                {"Texture", "texture", -100, 100, 0},
                {"Clarity", "clarity", -100, 100, 0},
                {"Dehaze", "dehaze", -100, 100, 0},
                {"Glow", "glow", 0, 100, 0},
                {"Glow Range", "glowRange", -100, 100, 0},
                {"Glow Spread", "glowSpread", -100, 100, 0},
                {"Glow Warmth", "glowWarmth", -100, 100, 0},
                {"Vignette", "vignetteAmount", -100, 100, 0},
                {"Vignette Midpoint", "vignetteMidpoint", 0, 100, 0},
                {"Vignette Roundness", "vignetteRoundness", -100, 100, 0},
                {"Vignette Feather", "vignetteFeather", 0, 100, 0},
                {"Grain Amount", "grainAmount", 0, 100, 0},
                {"Grain Size", "grainSize", 0, 100, 0},
                {"Grain Roughness", "grainRoughness", 0, 100, 0}};
    }
    (void)value;
    return {};
}

[[nodiscard]] double field_default(const QString& kind, const char* key, const json& value) {
    if (kind == QStringLiteral("Hue/Saturation")) {
        return jnum(value, key, 0);
    }
    if (kind == QStringLiteral("Levels")) {
        const json& levels = jchild(value, "levels");
        if (levels.contains("ranges") && levels.at("ranges").is_array() && !levels.at("ranges").empty()) {
            const json& range = levels.at("ranges").at(0);
            return jnum(range, key, std::string(key) == "white" ? 255.0 : std::string(key) == "gamma" ? 1.0 : 0.0);
        }
        return std::string(key) == "white" ? 255.0 : std::string(key) == "gamma" ? 1.0 : 0.0;
    }
    if (kind == QStringLiteral("Exposure")) {
        return jnum(jchild(value, "exposureSettings"), key, std::string(key) == "gamma" ? 1.0 : 0.0);
    }
    if (kind == QStringLiteral("Grain")) {
        return jnum(jchild(value, "grainSettings"), key,
                    std::string(key) == "size"        ? 1.5
                    : std::string(key) == "roughness" ? 50.0
                    : std::string(key) == "amount"    ? 25.0
                                                      : 0.0);
    }
    return jnum(value, key, 0.0);
}

[[nodiscard]] json color_json(double red, double green, double blue) {
    return json{{"red", red}, {"green", green}, {"blue", blue}};
}

}  // namespace

const std::vector<QString>& filter_kinds() {
    static const std::vector<QString> kinds = [] {
        std::vector<QString> list = adjustment_kinds();
        list.emplace_back(QStringLiteral("Camera Raw"));
        list.emplace_back(QStringLiteral("Dither"));
        return list;
    }();
    return kinds;
}

bool prompt_dither(QWidget* parent, json& value) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Dither"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    layout->addLayout(form);

    auto* style = new QComboBox(&dialog);
    style->addItems({QStringLiteral("Atkinson"), QStringLiteral("Floyd-Steinberg"), QStringLiteral("Bayer 2"),
                     QStringLiteral("Bayer 4"), QStringLiteral("Bayer 8"), QStringLiteral("Dots"),
                     QStringLiteral("Lines"), QStringLiteral("Diamonds"), QStringLiteral("Patterns"),
                     QStringLiteral("Scanlines")});
    const QString current = QString::fromStdString(value.value("style", std::string("Atkinson")));
    if (style->findText(current) >= 0) {
        style->setCurrentText(current);
    }
    form->addRow(QStringLiteral("Style"), style);

    auto* levels = new QDoubleSpinBox(&dialog);
    levels->setRange(2, 16);
    levels->setDecimals(0);
    levels->setValue(value.value("levels", 2.0));
    form->addRow(QStringLiteral("Levels"), levels);

    auto* diffusion = new QDoubleSpinBox(&dialog);
    diffusion->setRange(0.0, 1.0);
    diffusion->setDecimals(2);
    diffusion->setValue(value.value("diffusion", 1.0));
    form->addRow(QStringLiteral("Diffusion"), diffusion);

    auto* density = new QDoubleSpinBox(&dialog);
    density->setRange(-1.0, 1.0);
    density->setDecimals(2);
    density->setValue(value.value("density", 0.0));
    form->addRow(QStringLiteral("Density"), density);

    auto* contrast = new QDoubleSpinBox(&dialog);
    contrast->setRange(-1.0, 1.0);
    contrast->setDecimals(2);
    contrast->setValue(value.value("contrast", 0.0));
    form->addRow(QStringLiteral("Contrast"), contrast);

    auto* cell = new QDoubleSpinBox(&dialog);
    cell->setRange(2, 64);
    cell->setDecimals(0);
    cell->setValue(value.value("cell", 6.0));
    form->addRow(QStringLiteral("Cell (px)"), cell);

    auto* angle = new QDoubleSpinBox(&dialog);
    angle->setRange(-180.0, 180.0);
    angle->setDecimals(0);
    angle->setValue(value.value("angleDegrees", 45.0));
    form->addRow(QStringLiteral("Screen angle"), angle);

    auto* light_on_dark = new QCheckBox(QStringLiteral("Light on dark"), &dialog);
    light_on_dark->setChecked(value.value("lightOnDark", false));
    layout->addWidget(light_on_dark);
    auto* original = new QCheckBox(QStringLiteral("Original colors"), &dialog);
    original->setChecked(value.value("originalColors", false));
    layout->addWidget(original);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    arrange_tool_panel(dialog, 380);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    value["kind"] = std::string("Dither");
    value["style"] = style->currentText().toStdString();
    value["levels"] = levels->value();
    value["diffusion"] = diffusion->value();
    value["density"] = density->value();
    value["contrast"] = contrast->value();
    value["cell"] = cell->value();
    value["angleDegrees"] = angle->value();
    value["lightOnDark"] = light_on_dark->isChecked();
    value["originalColors"] = original->isChecked();
    return true;
}

const std::vector<QString>& adjustment_kinds() {
    static const std::vector<QString> kinds{
        QStringLiteral("Hue/Saturation"), QStringLiteral("Levels"),        QStringLiteral("Curves"),
        QStringLiteral("Exposure"),       QStringLiteral("Gradient Map"),  QStringLiteral("Grain"),
        QStringLiteral("Invert"),         QStringLiteral("Black & White"), QStringLiteral("Color Balance"),
        QStringLiteral("Gaussian Blur"),  QStringLiteral("Motion Blur"),   QStringLiteral("Add Noise")};
    return kinds;
}

bool prompt_adjustment(QWidget* parent, const QString& kind, const QString& title, nlohmann::json& value) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    layout->addLayout(form);

    std::map<std::string, QDoubleSpinBox*> spins;
    for (const Field& field : fields_for(kind, value)) {
        auto* spin = new QDoubleSpinBox(&dialog);
        spin->setRange(field.minimum, field.maximum);
        spin->setDecimals(field.decimals);
        spin->setValue(field_default(kind, field.key, value));
        form->addRow(QString::fromUtf8(field.label), spin);
        spins[field.key] = spin;
    }

    QPushButton* shadows_button = nullptr;
    QPushButton* highlights_button = nullptr;
    QColor shadows_color(0, 0, 0);
    QColor highlights_color(255, 255, 255);
    if (kind == QStringLiteral("Gradient Map")) {
        const json& settings = jchild(value, "gradientMapSettings");
        const auto read_color = [](const json& object, QColor fallback) {
            if (!object.is_object()) {
                return fallback;
            }
            return QColor(static_cast<int>(jnum(object, "red", 0) * 255),
                          static_cast<int>(jnum(object, "green", 0) * 255),
                          static_cast<int>(jnum(object, "blue", 0) * 255));
        };
        shadows_color = read_color(jchild(settings, "shadows"), QColor(0, 0, 0));
        highlights_color = read_color(jchild(settings, "highlights"), QColor(255, 255, 255));
        shadows_button = new QPushButton(&dialog);
        shadows_button->setStyleSheet(QStringLiteral("background-color: %1").arg(shadows_color.name()));
        highlights_button = new QPushButton(&dialog);
        highlights_button->setStyleSheet(QStringLiteral("background-color: %1").arg(highlights_color.name()));
        QObject::connect(shadows_button, &QPushButton::clicked, &dialog, [&] {
            const QColor chosen = QColorDialog::getColor(shadows_color, &dialog);
            if (chosen.isValid()) {
                shadows_color = chosen;
                shadows_button->setStyleSheet(QStringLiteral("background-color: %1").arg(chosen.name()));
            }
        });
        QObject::connect(highlights_button, &QPushButton::clicked, &dialog, [&] {
            const QColor chosen = QColorDialog::getColor(highlights_color, &dialog);
            if (chosen.isValid()) {
                highlights_color = chosen;
                highlights_button->setStyleSheet(QStringLiteral("background-color: %1").arg(chosen.name()));
            }
        });
        form->addRow(QStringLiteral("Shadows"), shadows_button);
        form->addRow(QStringLiteral("Highlights"), highlights_button);
    }

    QCheckBox* gaussian = nullptr;
    QCheckBox* monochromatic = nullptr;
    if (kind == QStringLiteral("Add Noise")) {
        gaussian = new QCheckBox(QStringLiteral("Gaussian"), &dialog);
        gaussian->setChecked(value.is_object() && value.value("noiseGaussian", false));
        monochromatic = new QCheckBox(QStringLiteral("Monochromatic"), &dialog);
        monochromatic->setChecked(value.is_object() && value.value("noiseMonochromatic", false));
        form->addRow(QString(), gaussian);
        form->addRow(QString(), monochromatic);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    arrange_tool_panel(dialog, 380);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const double hue = spins.count("hue") != 0 ? spins["hue"]->value() : 0.0;
    const double saturation = spins.count("saturation") != 0 ? spins["saturation"]->value() : 0.0;
    const double lightness = spins.count("lightness") != 0 ? spins["lightness"]->value() : 0.0;

    if (kind == QStringLiteral("Hue/Saturation")) {
        value = json{{"kind", "Hue/Saturation"}, {"hue", hue}, {"saturation", saturation}, {"lightness", lightness}};
    } else if (kind == QStringLiteral("Levels")) {
        const json range = json{{"black", spins["black"]->value()},
                                {"gamma", spins["gamma"]->value()},
                                {"white", spins["white"]->value()},
                                {"outputBlack", 0.0},
                                {"outputWhite", 255.0}};
        value = json{{"kind", "Levels"}, {"levels", {{"ranges", json::array({range, range, range, range})}}}};
    } else if (kind == QStringLiteral("Exposure")) {
        value = json{{"kind", "Exposure"},
                     {"exposureSettings",
                      {{"exposure", spins["exposure"]->value()},
                       {"offset", spins["offset"]->value()},
                       {"gamma", spins["gamma"]->value()}}}};
    } else if (kind == QStringLiteral("Grain")) {
        value = json{{"kind", "Grain"},
                     {"grainSettings",
                      {{"amount", spins["amount"]->value()},
                       {"size", spins["size"]->value()},
                       {"roughness", spins["roughness"]->value()},
                       {"seed", static_cast<std::uint32_t>(spins["seed"]->value())}}}};
    } else if (kind == QStringLiteral("Gaussian Blur")) {
        value = json{{"kind", "Gaussian Blur"}, {"blurRadius", spins["blurRadius"]->value()}};
    } else if (kind == QStringLiteral("Motion Blur")) {
        value = json{{"kind", "Motion Blur"},
                     {"motionDistance", spins["motionDistance"]->value()},
                     {"motionAngle", spins["motionAngle"]->value()}};
    } else if (kind == QStringLiteral("Add Noise")) {
        value = json{{"kind", "Add Noise"},
                     {"noiseAmount", spins["noiseAmount"]->value()},
                     {"noiseGaussian", gaussian != nullptr && gaussian->isChecked()},
                     {"noiseMonochromatic", monochromatic != nullptr && monochromatic->isChecked()}};
    } else if (kind == QStringLiteral("Gradient Map")) {
        value = json{
            {"kind", "Gradient Map"},
            {"gradientMapSettings",
             {{"shadows", color_json(shadows_color.redF(), shadows_color.greenF(), shadows_color.blueF())},
              {"highlights", color_json(highlights_color.redF(), highlights_color.greenF(), highlights_color.blueF())},
              {"reversed", false}}}};
    } else if (kind == QStringLiteral("Camera Raw")) {
        json out = json{{"kind", "Camera Raw"}};
        for (const Field& field : fields_for(kind, value)) {
            out[field.key] = spins[field.key]->value();
        }
        value = out;
    } else {
        value = json{{"kind", kind.toStdString()}};
    }
    return true;
}

}  // namespace compositor::appwin
