#include "hsv_dialog.hpp"

#include <QConicalGradient>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>

#include "tool_panel.hpp"

namespace compositor::appwin {
namespace {

/// A hue/saturation wheel: hue is the angle, saturation the radius. Dragging sets the adjustment's
/// hue shift (??180) and saturation shift (0???00), as Photoshop's Hue/Saturation does.
class HueWheel : public QWidget {
public:
    explicit HueWheel(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(200, 200);
        setMouseTracking(true);
    }

    void set_values(double hue, double saturation) {
        hue_ = hue;
        saturation_ = saturation;
        update();
    }
    [[nodiscard]] double hue() const { return hue_; }
    [[nodiscard]] double saturation() const { return saturation_; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPointF center(width() / 2.0, height() / 2.0);
        const double radius = std::min(width(), height()) / 2.0 - 8.0;
        QConicalGradient hue_gradient(center, 90.0);
        for (int i = 0; i <= 12; ++i) {
            hue_gradient.setColorAt(i / 12.0, QColor::fromHsvF((i % 12) / 12.0, 1.0, 1.0));
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(hue_gradient);
        painter.drawEllipse(center, radius, radius);
        QRadialGradient white(center, radius);
        white.setColorAt(0.0, QColor(255, 255, 255, 255));
        white.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.setBrush(white);
        painter.drawEllipse(center, radius, radius);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(40, 40, 40));
        painter.drawEllipse(center, radius, radius);

        const double angle = (hue_ + 180.0) / 360.0 * 2.0 * 3.14159265358979323846;
        const double reach = std::clamp(saturation_ / 100.0, 0.0, 1.0) * radius;
        const QPointF marker(center.x() + std::cos(angle) * reach, center.y() + std::sin(angle) * reach);
        painter.setPen(QPen(Qt::white, 3));
        painter.drawEllipse(marker, 6, 6);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawEllipse(marker, 6, 6);
    }

    void mousePressEvent(QMouseEvent* event) override { pick(event->position()); }
    void mouseMoveEvent(QMouseEvent* event) override {
        if ((event->buttons() & Qt::LeftButton) != 0) {
            pick(event->position());
        }
    }

private:
    void pick(const QPointF& position) {
        const QPointF center(width() / 2.0, height() / 2.0);
        const double radius = std::max(1.0, std::min(width(), height()) / 2.0 - 8.0);
        const double dx = position.x() - center.x();
        const double dy = position.y() - center.y();
        const double distance = std::hypot(dx, dy);
        double degrees = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;
        if (degrees < 0.0) {
            degrees += 360.0;
        }
        hue_ = degrees - 180.0;
        saturation_ = std::clamp(distance / radius, 0.0, 1.0) * 100.0;
        if (changed_) {
            changed_(hue_, saturation_);
        }
        update();
    }

public:
    std::function<void(double, double)> changed_;

private:
    double hue_ = 0.0;
    double saturation_ = 0.0;
};

[[nodiscard]] double field(const nlohmann::json& value, const char* key, double fallback) {
    return value.is_object() && value.contains(key) && value.at(key).is_number() ? value.at(key).get<double>()
                                                                                 : fallback;
}

}  // namespace

bool prompt_hsv(QWidget* parent, nlohmann::json& value) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Hue/Saturation"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* wheel = new HueWheel(&dialog);
    wheel->set_values(field(value, "hue", 0.0), field(value, "saturation", 0.0));
    layout->addWidget(wheel);

    auto* lightness = new QSlider(Qt::Horizontal, &dialog);
    lightness->setRange(-100, 100);
    lightness->setValue(static_cast<int>(std::lround(field(value, "lightness", 0.0))));
    auto* label = new QLabel(QStringLiteral("Lightness %1").arg(lightness->value()), &dialog);
    QObject::connect(lightness, &QSlider::valueChanged, label,
                     [label](int v) { label->setText(QStringLiteral("Lightness %1").arg(v)); });
    layout->addWidget(label);
    layout->addWidget(lightness);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    arrange_tool_panel(dialog, 320);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    value = nlohmann::json{{"kind", "Hue/Saturation"},
                           {"hue", wheel->hue()},
                           {"saturation", wheel->saturation()},
                           {"lightness", static_cast<double>(lightness->value())}};
    return true;
}

}  // namespace compositor::appwin
