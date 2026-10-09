#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <algorithm>
#include <cmath>

#include "canvas_view.hpp"

// The canvas overlays — selection outline, lasso, gradient preview, layout grid and guides — split
// out so `canvas_view.cpp` stays within the size limit.

namespace compositor::appwin {

namespace {

/// Numbered ruler ticks about 70 points apart, using 1-2-5 steps in document pixels, as the macOS
/// app's `CanvasRuler.majorStep` does.
[[nodiscard]] double ruler_major_step(double scale) {
    static const double kNice[] = {1.0,   2.0,   5.0,    10.0,   20.0,   25.0,   50.0,    100.0,   200.0,
                                   250.0, 500.0, 1000.0, 2000.0, 2500.0, 5000.0, 10000.0, 20000.0, 25000.0};
    const double target = 70.0 / std::max(scale, 0.0001);
    for (const double candidate : kNice) {
        if (candidate >= target) {
            return candidate;
        }
    }
    return 50000.0;
}

}  // namespace

void CanvasView::draw_rulers(QPainter& painter) {
    const double thickness = ruler_thickness();
    painter.fillRect(QRectF(0.0, 0.0, width(), thickness), QColor(51, 51, 51));
    painter.fillRect(QRectF(0.0, 0.0, thickness, height()), QColor(51, 51, 51));
    if (image_.isNull()) {
        return;
    }
    const double scale = current_scale();
    if (scale <= 0.0) {
        return;
    }
    const QRectF viewport = viewport_rect();
    const double step = ruler_major_step(scale);
    const double minor = step / 10.0;
    QFont font;
    font.setPixelSize(9);
    painter.setFont(font);

    const QPointF top_left = to_document(QPointF(viewport.left(), viewport.top()));
    const QPointF bottom_right = to_document(QPointF(viewport.right(), viewport.bottom()));
    const auto tick = [&](double value, double major_length) {
        const double remainder = std::fabs(std::fmod(value, step));
        if (remainder < 1e-3 || std::fabs(remainder - step) < 1e-3) {
            return major_length;
        }
        return std::fabs(std::fmod(value, step / 2.0)) < 1e-3 ? 5.0 : 3.0;
    };

    for (double value = std::floor(top_left.x() / minor) * minor;
         value <= std::ceil(bottom_right.x() / minor) * minor + 1e-6; value += minor) {
        const double wx = to_widget(QPointF(value, 0.0)).x();
        if (wx < viewport.left() - 1.0 || wx > viewport.right() + 1.0) {
            continue;
        }
        const double length = tick(value, 8.0);
        painter.setPen(QColor(158, 158, 158));
        painter.drawLine(QPointF(wx, thickness), QPointF(wx, thickness - length));
        if (length >= 8.0) {
            painter.setPen(QColor(199, 199, 199));
            painter.drawText(QPointF(wx + 2.0, thickness - 2.0),
                             QString::number(static_cast<long long>(std::llround(value))));
        }
    }
    painter.setPen(QColor(20, 20, 20));
    painter.drawLine(QPointF(0.0, thickness), QPointF(width(), thickness));

    for (double value = std::floor(top_left.y() / minor) * minor;
         value <= std::ceil(bottom_right.y() / minor) * minor + 1e-6; value += minor) {
        const double wy = to_widget(QPointF(0.0, value)).y();
        if (wy < viewport.top() - 1.0 || wy > viewport.bottom() + 1.0) {
            continue;
        }
        const double length = tick(value, 8.0);
        painter.setPen(QColor(158, 158, 158));
        painter.drawLine(QPointF(thickness, wy), QPointF(thickness - length, wy));
        if (length >= 8.0) {
            painter.setPen(QColor(199, 199, 199));
            painter.save();
            painter.translate(QPointF(2.0, wy + 2.0));
            painter.rotate(-90.0);
            painter.drawText(QPointF(-30.0, 0.0), QString::number(static_cast<long long>(std::llround(value))));
            painter.restore();
        }
    }
    painter.setPen(QColor(20, 20, 20));
    painter.drawLine(QPointF(thickness, 0.0), QPointF(thickness, height()));
}

void CanvasView::set_guides(std::vector<std::pair<int, double>> guides) {
    guides_ = std::move(guides);
    update();
}

void CanvasView::set_grid(bool visible, int spacing, int subdivisions) {
    grid_visible_ = visible;
    grid_spacing_ = std::max(1, spacing);
    grid_subdivisions_ = std::max(1, subdivisions);
    update();
}

void CanvasView::draw_overlays(QPainter& painter) {
    if (image_.isNull()) {
        return;
    }
    painter.setBrush(Qt::NoBrush);

    if (grid_visible_) {
        QPen grid_pen(QColor(255, 255, 255, 55));
        grid_pen.setWidth(1);
        painter.setPen(grid_pen);
        const double step = static_cast<double>(grid_spacing_) / grid_subdivisions_;
        for (double x = 0.0; x <= image_.width(); x += step) {
            painter.drawLine(to_widget(QPointF(x, 0.0)), to_widget(QPointF(x, image_.height())));
        }
        for (double y = 0.0; y <= image_.height(); y += step) {
            painter.drawLine(to_widget(QPointF(0.0, y)), to_widget(QPointF(image_.width(), y)));
        }
    }

    const auto draw_dashed = [&painter, this](const QPolygonF& polygon) {
        if (polygon.size() < 2) {
            return;
        }
        QPen under(Qt::black);
        under.setWidth(3);
        painter.setPen(under);
        painter.drawPolygon(polygon);
        QPen ants(Qt::white);
        ants.setWidth(1);
        ants.setStyle(Qt::DashLine);
        painter.setPen(ants);
        painter.drawPolygon(polygon);
    };
    for (const std::vector<QPointF>& loop : selection_outline_) {
        QPolygonF polygon;
        for (const QPointF& point : loop) {
            polygon << to_widget(point);
        }
        draw_dashed(polygon);
    }
    if (lasso_points_.size() >= 2) {
        QPolygonF polygon;
        for (const QPointF& point : lasso_points_) {
            polygon << to_widget(point);
        }
        draw_dashed(polygon);
    }
    if (polygon_points_.size() >= 2) {
        QPolygonF polygon;
        for (const QPointF& point : polygon_points_) {
            polygon << to_widget(point);
        }
        draw_dashed(polygon);
    }
    if (!guides_.empty()) {
        QPen guide_pen(QColor(0, 255, 255, 200));
        guide_pen.setWidth(1);
        guide_pen.setStyle(Qt::DashLine);
        painter.setPen(guide_pen);
        for (const auto& [axis, position] : guides_) {
            if (axis == 0) {
                painter.drawLine(to_widget(QPointF(0.0, position)), to_widget(QPointF(image_.width(), position)));
            } else {
                painter.drawLine(to_widget(QPointF(position, 0.0)), to_widget(QPointF(position, image_.height())));
            }
        }
    }
    if (!crop_rect_.isNull()) {
        const QRectF outline(to_widget(crop_rect_.topLeft()), to_widget(crop_rect_.bottomRight()));
        QPen pen(Qt::white);
        pen.setWidth(2);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.drawRect(outline);
    }
    if (gradienting_) {
        QPen pen(Qt::white);
        pen.setWidth(2);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.drawLine(to_widget(gradient_start_), to_widget(gradient_end_));
    }
    if (dragging_guide_) {
        QPen pen(QColor(0, 255, 255, 220));
        pen.setWidth(1);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        if (guide_horizontal_) {
            painter.drawLine(to_widget(QPointF(0.0, guide_position_)),
                             to_widget(QPointF(image_.width(), guide_position_)));
        } else {
            painter.drawLine(to_widget(QPointF(guide_position_, 0.0)),
                             to_widget(QPointF(guide_position_, image_.height())));
        }
    }
    if (distort_mode_ && distort_quad_.size() == 4) {
        QPolygonF polygon;
        for (const QPointF& corner : distort_quad_) {
            polygon << to_widget(corner);
        }
        QPen outline(Qt::white);
        outline.setWidth(1);
        outline.setStyle(Qt::DashLine);
        painter.setPen(outline);
        painter.drawPolygon(polygon);
        painter.setBrush(QColor(42, 130, 218));
        for (const QPointF& corner : distort_quad_) {
            const QPointF center = to_widget(corner);
            painter.drawRect(QRectF(center.x() - 4.0, center.y() - 4.0, 8.0, 8.0));
        }
        painter.setBrush(Qt::NoBrush);
    }
    if (hovering_ && brush_diameter_ > 0.0 && (paint_mode_ || heal_mode_ || clone_mode_)) {
        const double radius = brush_diameter_ * current_scale() / 2.0;
        if (radius >= 1.0) {
            QPen outline(Qt::white);
            outline.setWidthF(2.5);
            painter.setPen(outline);
            painter.drawEllipse(hover_pos_, radius, radius);
            QPen inner(Qt::black);
            inner.setWidthF(1.0);
            painter.setPen(inner);
            painter.drawEllipse(hover_pos_, radius, radius);
        }
    }
}

}  // namespace compositor::appwin
