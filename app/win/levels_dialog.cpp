#include "levels_dialog.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "tool_panel.hpp"

namespace compositor::appwin {
namespace {

class HistogramWidget : public QWidget {
public:
    HistogramWidget(std::array<double, 1024> bins, QWidget* parent = nullptr)
        : QWidget(parent), bins_(std::move(bins)) {
        setMinimumSize(280, 120);
    }

    [[nodiscard]] QSize sizeHint() const override { return QSize(300, 130); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(28, 28, 28));
        const QRectF area = rect().adjusted(4, 4, -4, -4);
        double peak = 1.0;
        for (int i = 0; i < 256; ++i) {
            peak = std::max(peak, bins_[static_cast<std::size_t>(i)]);
        }
        const auto channel = [&](int channel_index, const QColor& color, double alpha) {
            QPainterPath path;
            path.moveTo(area.left(), area.bottom());
            for (int i = 0; i < 256; ++i) {
                const double value =
                    bins_[static_cast<std::size_t>(channel_index) * 256U + static_cast<std::size_t>(i)];
                const double x = area.left() + (i / 255.0) * area.width();
                const double y = area.bottom() - std::min(1.0, value / peak) * area.height();
                path.lineTo(x, y);
            }
            path.lineTo(area.right(), area.bottom());
            path.closeSubpath();
            QColor fill = color;
            fill.setAlphaF(static_cast<float>(alpha));
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawPath(path);
        };
        channel(1, QColor(220, 70, 70), 0.5);
        channel(2, QColor(70, 200, 70), 0.5);
        channel(3, QColor(90, 140, 240), 0.5);
        channel(0, QColor(230, 230, 230), 0.45);
        painter.setPen(QColor(70, 70, 70));
        painter.drawRect(area);
    }

private:
    std::array<double, 1024> bins_;
};

[[nodiscard]] double range_value(const nlohmann::json& value, const char* key, double fallback) {
    if (value.is_object() && value.contains("levels") && value.at("levels").is_object()) {
        const nlohmann::json& levels = value.at("levels");
        if (levels.contains("ranges") && levels.at("ranges").is_array() && !levels.at("ranges").empty()) {
            const nlohmann::json& range = levels.at("ranges").at(0);
            if (range.is_object() && range.contains(key) && range.at(key).is_number()) {
                return range.at(key).get<double>();
            }
        }
    }
    return fallback;
}

}  // namespace

bool prompt_levels(QWidget* parent, const std::array<double, 1024>& bins, nlohmann::json& value) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Levels"));
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new HistogramWidget(bins, &dialog));
    auto* form = new QFormLayout();

    auto* black = new QDoubleSpinBox(&dialog);
    black->setRange(0.0, 254.0);
    black->setDecimals(0);
    black->setValue(range_value(value, "black", 0.0));
    form->addRow(QStringLiteral("Black"), black);

    auto* gamma = new QDoubleSpinBox(&dialog);
    gamma->setRange(0.10, 9.99);
    gamma->setDecimals(2);
    gamma->setValue(range_value(value, "gamma", 1.0));
    form->addRow(QStringLiteral("Gamma"), gamma);

    auto* white = new QDoubleSpinBox(&dialog);
    white->setRange(1.0, 255.0);
    white->setDecimals(0);
    white->setValue(range_value(value, "white", 255.0));
    form->addRow(QStringLiteral("White"), white);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    arrange_tool_panel(dialog, 380);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    const nlohmann::json range = {{"black", black->value()},
                                  {"gamma", gamma->value()},
                                  {"white", white->value()},
                                  {"outputBlack", 0.0},
                                  {"outputWhite", 255.0}};
    value = nlohmann::json{{"kind", "Levels"},
                           {"levels", {{"ranges", nlohmann::json::array({range, range, range, range})}}}};
    return true;
}

}  // namespace compositor::appwin
