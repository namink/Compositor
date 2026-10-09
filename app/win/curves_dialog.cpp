#include "curves_dialog.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "tool_panel.hpp"

// A monotone cubic-curve graph editor for the Curves adjustment, matching the curve the core renders
// (`CurvesSettings.value`): shape-preserving, so it never overshoots between control points.

namespace compositor::appwin {
namespace {

struct CurvePoint {
    double x = 0.0;
    double y = 0.0;
};

[[nodiscard]] double curve_value(const std::vector<CurvePoint>& points, double x) {
    if (points.size() < 2) {
        return x;
    }
    std::vector<double> slopes(points.size() - 1);
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        slopes[i] = (points[i + 1].y - points[i].y) / (points[i + 1].x - points[i].x);
    }
    const auto slope = [&](std::size_t j) {
        if (j == 0) {
            return slopes.front();
        }
        if (j == points.size() - 1) {
            return slopes.back();
        }
        if (slopes[j - 1] * slopes[j] <= 0.0) {
            return 0.0;
        }
        return 2.0 / (1.0 / slopes[j - 1] + 1.0 / slopes[j]);
    };
    std::size_t i = 0;
    for (std::size_t j = 0; j + 1 < points.size(); ++j) {
        if (points[j].x <= x) {
            i = j;
        }
    }
    i = std::min(points.size() - 2, i);
    const double h = points[i + 1].x - points[i].x;
    const double t = std::clamp(h == 0.0 ? 0.0 : (x - points[i].x) / h, 0.0, 1.0);
    const double y = (2 * t * t * t - 3 * t * t + 1) * points[i].y + (t * t * t - 2 * t * t + t) * h * slope(i) +
                     (-2 * t * t * t + 3 * t * t) * points[i + 1].y + (t * t * t - t * t) * h * slope(i + 1);
    return std::clamp(y, 0.0, 255.0);
}

[[nodiscard]] QColor channel_color(int channel) {
    switch (channel) {
    case 1:
        return QColor(230, 70, 70);
    case 2:
        return QColor(70, 210, 70);
    case 3:
        return QColor(90, 140, 240);
    default:
        return QColor(235, 235, 235);
    }
}

class CurveCanvas : public QWidget {
public:
    CurveCanvas(std::array<std::vector<CurvePoint>, 4> channels, QWidget* parent = nullptr)
        : QWidget(parent), channels_(std::move(channels)) {
        setMinimumSize(260, 260);
        setMouseTracking(true);
    }

    void set_channel(int channel) {
        channel_ = channel;
        update();
    }

    void reset_channel() {
        channels_[static_cast<std::size_t>(channel_)] = {CurvePoint{0.0, 0.0}, CurvePoint{255.0, 255.0}};
        update();
    }

    [[nodiscard]] const std::array<std::vector<CurvePoint>, 4>& channels() const { return channels_; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(35, 35, 35));
        const QRectF area = rect().adjusted(6, 6, -6, -6);
        painter.setPen(QColor(60, 60, 60));
        for (int i = 1; i < 4; ++i) {
            const double t = static_cast<double>(i) / 4.0;
            painter.drawLine(QPointF(area.left() + area.width() * t, area.top()),
                             QPointF(area.left() + area.width() * t, area.bottom()));
            painter.drawLine(QPointF(area.left(), area.top() + area.height() * t),
                             QPointF(area.right(), area.top() + area.height() * t));
        }
        painter.setPen(QColor(80, 80, 80));
        painter.drawLine(area.bottomLeft(), area.topRight());

        QPainterPath path;
        for (int i = 0; i <= 255; ++i) {
            const double value = curve_value(channels_[static_cast<std::size_t>(channel_)], i);
            const QPointF point = to_widget(static_cast<double>(i), value);
            if (i == 0) {
                path.moveTo(point);
            } else {
                path.lineTo(point);
            }
        }
        painter.setPen(channel_color(channel_));
        painter.drawPath(path);
        painter.setBrush(channel_color(channel_));
        for (const CurvePoint& point : channels_[static_cast<std::size_t>(channel_)]) {
            painter.drawEllipse(to_widget(point.x, point.y), 4.0, 4.0);
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) {
            return;
        }
        std::vector<CurvePoint>& points = channels_[static_cast<std::size_t>(channel_)];
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (QLineF(event->position(), to_widget(points[i].x, points[i].y)).length() < 10.0) {
                dragging_ = static_cast<int>(i);
                return;
            }
        }
        const CurvePoint added = from_widget(event->position());
        points.push_back(added);
        std::sort(points.begin(), points.end(), [](const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (points[i].x == added.x && points[i].y == added.y) {
                dragging_ = static_cast<int>(i);
            }
        }
        update();
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (dragging_ < 0) {
            return;
        }
        std::vector<CurvePoint>& points = channels_[static_cast<std::size_t>(channel_)];
        CurvePoint moved = from_widget(event->position());
        const std::size_t index = static_cast<std::size_t>(dragging_);
        if (index == 0) {
            moved.x = 0.0;
        } else if (index + 1 == points.size()) {
            moved.x = 255.0;
        } else {
            moved.x = std::clamp(moved.x, points[index - 1].x + 1.0, points[index + 1].x - 1.0);
        }
        points[index] = moved;
        update();
    }

    void mouseReleaseEvent(QMouseEvent*) override { dragging_ = -1; }

private:
    [[nodiscard]] QPointF to_widget(double x, double y) const {
        const QRectF area = rect().adjusted(6, 6, -6, -6);
        return QPointF(area.left() + (x / 255.0) * area.width(), area.bottom() - (y / 255.0) * area.height());
    }
    [[nodiscard]] CurvePoint from_widget(const QPointF& position) const {
        const QRectF area = rect().adjusted(6, 6, -6, -6);
        CurvePoint point;
        point.x = std::clamp((position.x() - area.left()) / area.width() * 255.0, 0.0, 255.0);
        point.y = std::clamp((area.bottom() - position.y()) / area.height() * 255.0, 0.0, 255.0);
        return point;
    }

    std::array<std::vector<CurvePoint>, 4> channels_;
    int channel_ = 0;
    int dragging_ = -1;
};

[[nodiscard]] std::vector<CurvePoint> read_channel(const nlohmann::json& list) {
    std::vector<CurvePoint> points;
    if (list.is_array()) {
        for (const nlohmann::json& item : list) {
            if (item.is_object()) {
                points.push_back(CurvePoint{item.value("x", 0.0), item.value("y", 0.0)});
            }
        }
    }
    if (points.size() < 2) {
        points = {CurvePoint{0.0, 0.0}, CurvePoint{255.0, 255.0}};
    }
    return points;
}

[[nodiscard]] nlohmann::json write_channel(const std::vector<CurvePoint>& points) {
    nlohmann::json list = nlohmann::json::array();
    for (const CurvePoint& point : points) {
        list.push_back({{"x", point.x}, {"y", point.y}});
    }
    return list;
}

}  // namespace

bool prompt_curves(QWidget* parent, nlohmann::json& value) {
    std::array<std::vector<CurvePoint>, 4> channels;
    const nlohmann::json& curves = value.is_object() && value.contains("curves") && value.at("curves").is_object()
                                       ? value.at("curves")
                                       : nlohmann::json::object();
    if (curves.contains("channels") && curves.at("channels").is_array()) {
        const nlohmann::json& lists = curves.at("channels");
        for (std::size_t c = 0; c < channels.size() && c < lists.size(); ++c) {
            channels[c] = read_channel(lists.at(c));
        }
    } else {
        for (std::vector<CurvePoint>& channel : channels) {
            channel = {CurvePoint{0.0, 0.0}, CurvePoint{255.0, 255.0}};
        }
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Curves"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* channel = new QComboBox(&dialog);
    channel->addItems({QStringLiteral("RGB"), QStringLiteral("Red"), QStringLiteral("Green"), QStringLiteral("Blue")});
    layout->addWidget(channel);
    auto* canvas = new CurveCanvas(channels, &dialog);
    layout->addWidget(canvas);
    auto* reset = new QPushButton(QStringLiteral("Reset Channel"), &dialog);
    QObject::connect(channel, &QComboBox::currentIndexChanged, canvas,
                     [canvas](int index) { canvas->set_channel(index); });
    QObject::connect(reset, &QPushButton::clicked, canvas, &CurveCanvas::reset_channel);
    layout->addWidget(reset);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    arrange_tool_panel(dialog, 380);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    nlohmann::json lists = nlohmann::json::array();
    for (const std::vector<CurvePoint>& curve : canvas->channels()) {
        lists.push_back(write_channel(curve));
    }
    value = nlohmann::json{{"kind", "Curves"}, {"curves", {{"channels", lists}}}};
    return true;
}

}  // namespace compositor::appwin
