#include "tool_icons.hpp"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace compositor::appwin {
namespace {

constexpr int kSize = 22;

void arrow_head(QPainter& painter, const QPointF& tip, double dx, double dy) {
    const double length = std::hypot(dx, dy);
    if (length <= 0.0) {
        return;
    }
    const double ux = dx / length;
    const double uy = dy / length;
    const double px = -uy;
    const double py = ux;
    const QPointF a(tip.x() - ux * 5 + px * 3, tip.y() - uy * 5 + py * 3);
    const QPointF b(tip.x() - ux * 5 - px * 3, tip.y() - uy * 5 - py * 3);
    painter.drawLine(tip, a);
    painter.drawLine(tip, b);
}

}  // namespace

QIcon tool_icon(const QString& tool) {
    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(224, 224, 224));
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (tool == QLatin1String("Move") || tool == QLatin1String("Pan")) {
        painter.drawLine(QPointF(11, 3), QPointF(11, 19));
        painter.drawLine(QPointF(3, 11), QPointF(19, 11));
        arrow_head(painter, QPointF(11, 2), 0, -1);
        arrow_head(painter, QPointF(11, 20), 0, 1);
        arrow_head(painter, QPointF(2, 11), -1, 0);
        arrow_head(painter, QPointF(20, 11), 1, 0);
    } else if (tool == QLatin1String("Rect")) {
        QPen dashed = pen;
        dashed.setStyle(Qt::DashLine);
        painter.setPen(dashed);
        painter.drawRect(QRectF(3.5, 5.5, 15, 11));
    } else if (tool == QLatin1String("Ellipse")) {
        QPen dashed = pen;
        dashed.setStyle(Qt::DashLine);
        painter.setPen(dashed);
        painter.drawEllipse(QRectF(3.5, 5.5, 15, 11));
    } else if (tool == QLatin1String("Lasso") || tool == QLatin1String("Poly Lasso")) {
        painter.drawEllipse(QRectF(5.5, 4.5, 11, 9));
        painter.drawLine(QPointF(7, 13), QPointF(4, 19));
    } else if (tool == QLatin1String("Wand")) {
        painter.drawLine(QPointF(5, 19), QPointF(16, 8));
        painter.drawLine(QPointF(17, 4), QPointF(19, 6));
        painter.drawLine(QPointF(18, 3), QPointF(18, 7));
        painter.drawLine(QPointF(15, 5), QPointF(19, 5));
    } else if (tool == QLatin1String("Eyedropper")) {
        painter.drawLine(QPointF(5, 19), QPointF(14, 10));
        painter.drawLine(QPointF(14, 10), QPointF(17, 7));
        painter.drawLine(QPointF(15, 5), QPointF(19, 9));
    } else if (tool == QLatin1String("Gradient")) {
        QLinearGradient gradient(3, 0, 19, 0);
        gradient.setColorAt(0.0, QColor(224, 224, 224));
        gradient.setColorAt(1.0, QColor(70, 70, 70));
        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        painter.drawRect(QRectF(3, 6, 16, 10));
    } else if (tool == QLatin1String("Rect Fill")) {
        painter.setBrush(QColor(224, 224, 224));
        painter.setPen(Qt::NoPen);
        painter.drawRect(QRectF(4, 6, 14, 10));
    } else if (tool == QLatin1String("Ellipse Fill")) {
        painter.setBrush(QColor(224, 224, 224));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(4, 6, 14, 10));
    } else if (tool == QLatin1String("Brush")) {
        QPen thick = pen;
        thick.setWidthF(3.0);
        painter.setPen(thick);
        painter.drawLine(QPointF(5, 18), QPointF(17, 4));
    } else if (tool == QLatin1String("Spot Heal")) {
        painter.drawRoundedRect(QRectF(5, 6, 12, 10), 3, 3);
        painter.drawLine(QPointF(11, 7), QPointF(11, 15));
        painter.drawLine(QPointF(7, 11), QPointF(15, 11));
    } else if (tool == QLatin1String("Clone Stamp")) {
        painter.drawEllipse(QRectF(4, 4, 8, 8));
        painter.drawLine(QPointF(12, 12), QPointF(18, 18));
    } else if (tool == QLatin1String("Blur")) {
        QPainterPath drop;
        drop.moveTo(11, 3);
        drop.cubicTo(17, 10, 18, 12, 17, 15);
        drop.cubicTo(16, 19, 6, 19, 5, 15);
        drop.cubicTo(4, 12, 5, 10, 11, 3);
        painter.drawPath(drop);
    } else if (tool == QLatin1String("Crop")) {
        painter.drawLine(QPointF(7, 3), QPointF(7, 15));
        painter.drawLine(QPointF(7, 15), QPointF(19, 15));
        painter.drawLine(QPointF(3, 7), QPointF(15, 7));
        painter.drawLine(QPointF(15, 7), QPointF(15, 19));
    } else if (tool == QLatin1String("Rectangle")) {
        painter.drawRoundedRect(QRectF(4, 5, 14, 12), 3, 3);
    } else if (tool == QLatin1String("Distort")) {
        painter.drawRect(QRectF(6, 6, 10, 10));
        painter.setBrush(QColor(224, 224, 224));
        const QPointF corners[4] = {QPointF(6, 6), QPointF(16, 6), QPointF(16, 16), QPointF(6, 16)};
        for (const QPointF& corner : corners) {
            painter.drawRect(QRectF(corner.x() - 2, corner.y() - 2, 4, 4));
        }
        painter.setBrush(Qt::NoBrush);
    } else if (tool == QLatin1String("Type")) {
        painter.drawLine(QPointF(6, 6), QPointF(16, 6));
        painter.drawLine(QPointF(11, 6), QPointF(11, 18));
    } else {
        // Fall back to the tool's first letter.
        QFont font = painter.font();
        font.setPixelSize(13);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, tool.left(1));
    }

    painter.end();
    return QIcon(pixmap);
}

QIcon panel_icon(const QString& name) {
    constexpr int kPanelSize = 18;
    QPixmap pixmap(kPanelSize, kPanelSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(210, 210, 210));
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (name == QLatin1String("plus")) {
        painter.drawLine(QPointF(9, 3), QPointF(9, 15));
        painter.drawLine(QPointF(3, 9), QPointF(15, 9));
    } else if (name == QLatin1String("folder")) {
        QPainterPath path;
        path.moveTo(2, 5);
        path.lineTo(7, 5);
        path.lineTo(8.5, 7);
        path.lineTo(16, 7);
        path.lineTo(16, 14);
        path.lineTo(2, 14);
        path.closeSubpath();
        painter.drawPath(path);
    } else if (name == QLatin1String("mask")) {
        painter.drawRect(QRectF(3, 3, 12, 12));
        painter.setBrush(QColor(210, 210, 210));
        painter.drawPie(QRectF(3, 3, 12, 12), 90 * 16, 180 * 16);
    } else if (name == QLatin1String("effects")) {
        QPainterPath star;
        star.moveTo(9, 2);
        star.lineTo(11, 7);
        star.lineTo(16, 9);
        star.lineTo(11, 11);
        star.lineTo(9, 16);
        star.lineTo(7, 11);
        star.lineTo(2, 9);
        star.lineTo(7, 7);
        star.closeSubpath();
        painter.drawPath(star);
    } else if (name == QLatin1String("adjustment")) {
        painter.drawEllipse(QPointF(9, 9), 6, 6);
        painter.setBrush(QColor(210, 210, 210));
        painter.drawPie(QRectF(3, 3, 12, 12), 90 * 16, -180 * 16);
    } else if (name == QLatin1String("trash")) {
        painter.drawLine(QPointF(4, 5), QPointF(14, 5));
        painter.drawLine(QPointF(7, 5), QPointF(7, 3));
        painter.drawLine(QPointF(7, 3), QPointF(11, 3));
        painter.drawLine(QPointF(11, 3), QPointF(11, 5));
        painter.drawRect(QRectF(5, 5, 8, 10));
    } else if (name == QLatin1String("history")) {
        painter.drawEllipse(QPointF(9, 9), 6, 6);
        painter.drawLine(QPointF(9, 9), QPointF(9, 5));
        painter.drawLine(QPointF(9, 9), QPointF(12, 10));
    } else if (name == QLatin1String("navigator")) {
        painter.drawEllipse(QPointF(9, 9), 6, 6);
        QPainterPath needle;
        needle.moveTo(12, 6);
        needle.lineTo(7, 8);
        needle.lineTo(6, 12);
        needle.lineTo(11, 10);
        needle.closeSubpath();
        painter.drawPath(needle);
    } else {  // layers
        painter.drawRect(QRectF(4, 3, 10, 8));
        painter.drawRect(QRectF(6, 7, 10, 8));
    }
    painter.end();
    return QIcon(pixmap);
}

}  // namespace compositor::appwin
