#include "camera_raw_color_dialog.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>
#include <array>
#include <memory>
#include <string>

#include "tool_panel.hpp"

namespace compositor::appwin {
namespace {

using nlohmann::json;

[[nodiscard]] double num(const json& object, const char* key, double fallback) {
    return object.is_object() && object.contains(key) && object.at(key).is_number() ? object.at(key).get<double>()
                                                                                    : fallback;
}

[[nodiscard]] double array_at(const json& object, const char* key, int index, double fallback) {
    if (object.is_object() && object.contains(key) && object.at(key).is_array()) {
        const json& list = object.at(key);
        if (static_cast<std::size_t>(index) < list.size() && list.at(index).is_number()) {
            return list.at(index).get<double>();
        }
    }
    return fallback;
}

[[nodiscard]] QDoubleSpinBox* spin(QWidget* parent, QFormLayout* form, const QString& label, double value, double low,
                                   double high) {
    auto* box = new QDoubleSpinBox(parent);
    box->setRange(low, high);
    box->setValue(value);
    form->addRow(label, box);
    return box;
}

const char* const kMixerNames[8] = {"Reds", "Oranges", "Yellows", "Greens", "Aquas", "Blues", "Purples", "Magentas"};
const char* const kWheels[4] = {"shadows", "midtones", "highlights", "global"};
const char* const kWheelNames[4] = {"Shadows", "Midtones", "Highlights", "Global"};

}  // namespace

QWidget* build_camera_raw_color_panel(QWidget* parent, json& style, std::function<void()> on_change) {
    if (!style.is_object()) {
        style = json::object();
    }
    style["kind"] = std::string("Camera Raw");
    if (!style.contains("curve") || !style.at("curve").is_object()) {
        style["curve"] = json::object();
    }
    if (!style.contains("mixer") || !style.at("mixer").is_object()) {
        style["mixer"] = json::object();
    }
    if (!style.contains("grading") || !style.at("grading").is_object()) {
        style["grading"] = json::object();
    }
    json& curve = style["curve"];
    json& mixer = style["mixer"];
    json& grading = style["grading"];

    auto* root = new QWidget(parent);
    auto* layout = new QVBoxLayout(root);
    auto* tabs = new QTabWidget(root);
    layout->addWidget(tabs);

    auto* curve_page = new QWidget(tabs);
    auto* curve_form = new QFormLayout(curve_page);
    QDoubleSpinBox* shadows =
        spin(curve_page, curve_form, QStringLiteral("Shadows"), num(curve, "shadows", 0.0), -100, 100);
    QDoubleSpinBox* darks = spin(curve_page, curve_form, QStringLiteral("Darks"), num(curve, "darks", 0.0), -100, 100);
    QDoubleSpinBox* lights =
        spin(curve_page, curve_form, QStringLiteral("Lights"), num(curve, "lights", 0.0), -100, 100);
    QDoubleSpinBox* highlights =
        spin(curve_page, curve_form, QStringLiteral("Highlights"), num(curve, "highlights", 0.0), -100, 100);
    QDoubleSpinBox* refine = spin(curve_page, curve_form, QStringLiteral("Refine Saturation"),
                                  num(curve, "refineSaturation", 0.0), -100, 100);
    tabs->addTab(curve_page, QStringLiteral("Curve"));

    auto* mixer_page = new QWidget(tabs);
    auto* mixer_grid = new QGridLayout(mixer_page);
    mixer_grid->addWidget(new QLabel(QStringLiteral("Hue")), 0, 1);
    mixer_grid->addWidget(new QLabel(QStringLiteral("Saturation")), 0, 2);
    mixer_grid->addWidget(new QLabel(QStringLiteral("Luminance")), 0, 3);
    std::array<std::array<QDoubleSpinBox*, 3>, 8> mixer_spins{};
    for (int i = 0; i < 8; ++i) {
        mixer_grid->addWidget(new QLabel(QString::fromUtf8(kMixerNames[i])), i + 1, 0);
        for (int c = 0; c < 3; ++c) {
            auto* box = new QDoubleSpinBox(mixer_page);
            box->setRange(-100, 100);
            const char* key = c == 0 ? "hue" : c == 1 ? "saturation" : "luminance";
            box->setValue(array_at(mixer, key, i, 0.0));
            mixer_spins[static_cast<std::size_t>(i)][static_cast<std::size_t>(c)] = box;
            mixer_grid->addWidget(box, i + 1, c + 1);
        }
    }
    tabs->addTab(mixer_page, QStringLiteral("Mixer"));

    auto* grade_page = new QWidget(tabs);
    auto* grade_form = new QFormLayout(grade_page);
    std::array<std::array<QDoubleSpinBox*, 3>, 4> grade_spins{};
    for (int w = 0; w < 4; ++w) {
        const json wheel = grading.contains(kWheels[w]) && grading.at(kWheels[w]).is_object() ? grading.at(kWheels[w])
                                                                                              : json::object();
        grade_form->addRow(new QLabel(QString::fromUtf8(kWheelNames[w])));
        grade_spins[static_cast<std::size_t>(w)][0] =
            spin(grade_page, grade_form, QStringLiteral("  Hue"), num(wheel, "hue", 0.0), 0, 360);
        grade_spins[static_cast<std::size_t>(w)][1] =
            spin(grade_page, grade_form, QStringLiteral("  Saturation"), num(wheel, "saturation", 0.0), 0, 100);
        grade_spins[static_cast<std::size_t>(w)][2] =
            spin(grade_page, grade_form, QStringLiteral("  Luminance"), num(wheel, "luminance", 0.0), -100, 100);
    }
    QDoubleSpinBox* blending =
        spin(grade_page, grade_form, QStringLiteral("Blending"), num(grading, "blending", 50.0), 0, 100);
    QDoubleSpinBox* balance =
        spin(grade_page, grade_form, QStringLiteral("Balance"), num(grading, "balance", 0.0), -100, 100);
    tabs->addTab(grade_page, QStringLiteral("Grading"));

    // One writer reads every control back into `style`, then notifies the caller (which previews).
    auto writer = std::make_shared<std::function<void()>>();
    *writer = [&style, shadows, darks, lights, highlights, refine, mixer_spins, grade_spins, blending, balance,
               on_change] {
        style["curve"]["shadows"] = shadows->value();
        style["curve"]["darks"] = darks->value();
        style["curve"]["lights"] = lights->value();
        style["curve"]["highlights"] = highlights->value();
        style["curve"]["refineSaturation"] = refine->value();
        json hue = json::array();
        json saturation = json::array();
        json luminance = json::array();
        for (int i = 0; i < 8; ++i) {
            hue.push_back(mixer_spins[static_cast<std::size_t>(i)][0]->value());
            saturation.push_back(mixer_spins[static_cast<std::size_t>(i)][1]->value());
            luminance.push_back(mixer_spins[static_cast<std::size_t>(i)][2]->value());
        }
        style["mixer"]["hue"] = hue;
        style["mixer"]["saturation"] = saturation;
        style["mixer"]["luminance"] = luminance;
        for (int w = 0; w < 4; ++w) {
            style["grading"][kWheels[w]] = {{"hue", grade_spins[static_cast<std::size_t>(w)][0]->value()},
                                            {"saturation", grade_spins[static_cast<std::size_t>(w)][1]->value()},
                                            {"luminance", grade_spins[static_cast<std::size_t>(w)][2]->value()}};
        }
        style["grading"]["blending"] = blending->value();
        style["grading"]["balance"] = balance->value();
        if (on_change) {
            on_change();
        }
    };
    const auto connect_box = [writer](QDoubleSpinBox* box) {
        QObject::connect(box, &QDoubleSpinBox::valueChanged, box, [writer](double) { (*writer)(); });
    };
    for (QDoubleSpinBox* box : {shadows, darks, lights, highlights, refine, blending, balance}) {
        connect_box(box);
    }
    for (auto& row : mixer_spins) {
        for (QDoubleSpinBox* box : row) {
            connect_box(box);
        }
    }
    for (auto& row : grade_spins) {
        for (QDoubleSpinBox* box : row) {
            connect_box(box);
        }
    }
    return root;
}

bool prompt_camera_raw_color(QWidget* parent, json& adjustment) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Camera Raw Curve / Color"));
    auto* layout = new QVBoxLayout(&dialog);
    json style = adjustment;
    layout->addWidget(build_camera_raw_color_panel(&dialog, style, nullptr));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    arrange_tool_panel(dialog, 440);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    adjustment = style;
    return true;
}

}  // namespace compositor::appwin
