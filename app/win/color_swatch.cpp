#include "color_swatch.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace compositor::appwin {

ColorSwatch::ColorSwatch(QWidget* parent) : QWidget(parent) {
    setFixedSize(34, 46);
    setToolTip(QStringLiteral("Foreground / background color (click / right-click, swap)"));
}

void ColorSwatch::set_foreground(const QColor& color) {
    foreground_ = color;
    update();
}

void ColorSwatch::set_background(const QColor& color) {
    background_ = color;
    update();
}

int ColorSwatch::square_size() const {
    return 18;
}
int ColorSwatch::foreground_y() const {
    return 4;
}
int ColorSwatch::background_y() const {
    return 22;
}
int ColorSwatch::swap_y() const {
    return 3;
}

void ColorSwatch::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const int size = square_size();

    // Background square (behind, offset down-left).
    const QRect bg(2, background_y(), size, size);
    painter.setPen(QColor(20, 20, 20));
    painter.setBrush(background_);
    painter.drawRect(bg);
    painter.setPen(QColor(160, 160, 160));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(bg);

    // Foreground square (on top, offset up-right).
    const QRect fg(14, foreground_y(), size, size);
    painter.setPen(QColor(20, 20, 20));
    painter.setBrush(foreground_);
    painter.drawRect(fg);
    painter.setPen(QColor(160, 160, 160));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(fg);

    // A small swap arrow at the top-right corner.
    painter.setPen(QPen(QColor(200, 200, 200), 1.2));
    const QPointF from(30, swap_y() + 2);
    const QPointF to(30, swap_y() + 9);
    painter.drawLine(from, to);
    QPainterPath head;
    head.moveTo(to.x() - 3, to.y() - 3);
    head.lineTo(to.x() + 3, to.y() - 3);
    head.lineTo(to.x(), to.y());
    head.closeSubpath();
    painter.setBrush(QColor(200, 200, 200));
    painter.drawPath(head);
}

void ColorSwatch::mousePressEvent(QMouseEvent* event) {
    const int size = square_size();
    const QRect fg(14, foreground_y(), size, size);
    const QRect bg(2, background_y(), size, size);
    const QRect swap(24, swap_y() - 2, 10, 14);
    if (swap.contains(event->pos())) {
        if (on_swap) {
            on_swap();
        }
        return;
    }
    if (event->button() == Qt::RightButton) {
        if (on_background) {
            on_background(background_);
        }
    } else if (bg.contains(event->pos())) {
        if (on_background) {
            on_background(background_);
        }
    } else if (fg.contains(event->pos()) || event->button() == Qt::LeftButton) {
        if (on_foreground) {
            on_foreground(foreground_);
        }
    }
}

}  // namespace compositor::appwin
