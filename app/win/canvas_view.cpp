#include "canvas_view.hpp"

#include <QEvent>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QTimer>
#include <QWheelEvent>
#include <algorithm>

namespace compositor::appwin {
namespace {

constexpr int kCheckerSize = 12;
constexpr double kZoomStep = 1.15;
constexpr double kMinZoom = 0.02;
constexpr double kMaxZoom = 32.0;

void paint_checkerboard(QPainter& painter, const QRect& area) {
    painter.fillRect(area, QColor(90, 90, 90));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(120, 120, 120));
    for (int y = area.top(); y < area.bottom(); y += kCheckerSize) {
        for (int x = area.left(); x < area.right(); x += kCheckerSize) {
            if (((x / kCheckerSize) + (y / kCheckerSize)) % 2 == 0) {
                painter.drawRect(x, y, kCheckerSize, kCheckerSize);
            }
        }
    }
}

}  // namespace

CanvasView::CanvasView(QWidget* parent) : QOpenGLWidget(parent) {
    setMinimumSize(320, 240);
    setMouseTracking(true);
    // Coalesce a fast drag: at most one expensive re-composite per frame.
    stroke_timer_ = new QTimer(this);
    stroke_timer_->setInterval(16);
    stroke_timer_->setSingleShot(true);
    connect(stroke_timer_, &QTimer::timeout, this, [this] { flush_stroke_move(); });
    update_cursor();
}

void CanvasView::setImage(const QImage& image) {
    image_ = image;
    fit_ = true;
    pan_ = QPointF();
    selection_outline_.clear();
    update();
    notify_view_changed();
    if (image_changed_handler_) {
        image_changed_handler_();
    }
}

void CanvasView::updateImage(const QImage& image) {
    image_ = image;
    update();
    if (image_changed_handler_) {
        image_changed_handler_();
    }
}

void CanvasView::clear() {
    image_ = QImage();
    update();
}

QRectF CanvasView::viewport_rect() const {
    const double inset = ruler_thickness();
    return QRectF(inset, inset, std::max(1.0, width() - inset), std::max(1.0, height() - inset));
}

double CanvasView::ruler_thickness() const {
    return rulers_visible_ ? 18.0 : 0.0;
}

double CanvasView::fit_scale() const {
    if (image_.isNull()) {
        return 1.0;
    }
    const QRectF viewport = viewport_rect();
    return std::min(viewport.width() / image_.width(), viewport.height() / image_.height());
}

double CanvasView::current_scale() const {
    return fit_ ? fit_scale() : zoom_;
}

double CanvasView::scale() const {
    return current_scale();
}

void CanvasView::zoom_in() {
    zoom_ = std::clamp(current_scale() * kZoomStep, kMinZoom, kMaxZoom);
    fit_ = false;
    update();
    notify_view_changed();
}

void CanvasView::zoom_out() {
    zoom_ = std::clamp(current_scale() / kZoomStep, kMinZoom, kMaxZoom);
    fit_ = false;
    update();
    notify_view_changed();
}

void CanvasView::zoom_fit() {
    fit_ = true;
    pan_ = QPointF();
    update();
    notify_view_changed();
}

void CanvasView::zoom_actual() {
    zoom_ = 1.0;
    fit_ = false;
    pan_ = QPointF();
    update();
    notify_view_changed();
}

void CanvasView::leaveEvent(QEvent* event) {
    hovering_ = false;
    update();
    QOpenGLWidget::leaveEvent(event);
}

void CanvasView::set_selection_outline(std::vector<std::vector<QPointF>> loops) {
    selection_outline_ = std::move(loops);
    update();
}

void CanvasView::update_cursor() {
    if (paint_mode_ || wand_mode_ || lasso_mode_ || select_mode_ || polygon_mode_ || eyedropper_mode_ ||
        gradient_mode_ || heal_mode_ || clone_mode_ || crop_mode_ || text_mode_ || distort_mode_) {
        setCursor(Qt::CrossCursor);
    } else if (move_mode_) {
        setCursor(Qt::SizeAllCursor);
    } else {
        setCursor(Qt::OpenHandCursor);
    }
}

QPointF CanvasView::to_document(const QPointF& widget) const {
    const double scale = current_scale();
    if (scale <= 0.0 || image_.isNull()) {
        return QPointF();
    }
    const QSizeF shown(image_.width() * scale, image_.height() * scale);
    const QRectF viewport = viewport_rect();
    const QPointF top_left(viewport.x() + viewport.width() / 2.0 - shown.width() / 2.0 + pan_.x(),
                           viewport.y() + viewport.height() / 2.0 - shown.height() / 2.0 + pan_.y());
    return QPointF((widget.x() - top_left.x()) / scale, (widget.y() - top_left.y()) / scale);
}

QPointF CanvasView::to_widget(const QPointF& document) const {
    const double scale = current_scale();
    const QSizeF shown(image_.width() * scale, image_.height() * scale);
    const QRectF viewport = viewport_rect();
    const QPointF top_left(viewport.x() + viewport.width() / 2.0 - shown.width() / 2.0 + pan_.x(),
                           viewport.y() + viewport.height() / 2.0 - shown.height() / 2.0 + pan_.y());
    return QPointF(top_left.x() + document.x() * scale, top_left.y() + document.y() * scale);
}

void CanvasView::paintGL() {
    QPainter painter(this);
    const QRectF viewport = viewport_rect();
    // A solid board around the document; the checkerboard shows only under the document, where its
    // transparency is.
    painter.fillRect(rect(), QColor(30, 30, 30));
    painter.setClipRect(viewport);
    if (!image_.isNull()) {
        const double scale = current_scale();
        const QSizeF shown(image_.width() * scale, image_.height() * scale);
        const QPointF top_left(viewport.x() + viewport.width() / 2.0 - shown.width() / 2.0 + pan_.x(),
                               viewport.y() + viewport.height() / 2.0 - shown.height() / 2.0 + pan_.y());
        const QRectF document(top_left, shown);
        painter.setClipRect(document);
        paint_checkerboard(painter, document.toRect());
        painter.setClipRect(viewport);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, scale < 4.0);
        painter.drawImage(document, image_);
        draw_overlays(painter);
    }
    painter.setClipping(false);
    if (rulers_visible_) {
        draw_rulers(painter);
    }
}

void CanvasView::wheelEvent(QWheelEvent* event) {
    if (image_.isNull()) {
        return;
    }
    if ((event->modifiers() & Qt::ControlModifier) != 0) {
        zoom_ = std::clamp(current_scale() * (event->angleDelta().y() > 0 ? kZoomStep : 1.0 / kZoomStep), kMinZoom,
                           kMaxZoom);
        fit_ = false;
    } else {
        pan_ += QPointF(event->angleDelta().x(), event->angleDelta().y());
    }
    update();
    notify_view_changed();
    event->accept();
}

void CanvasView::flush_stroke_move() {
    if (!stroke_pending_) {
        return;
    }
    stroke_pending_ = false;
    const QPointF doc = stroke_pending_pos_;
    switch (stroke_pending_mode_) {
    case 1:
        if (paint_move_) {
            paint_move_(doc.x(), doc.y());
        }
        break;
    case 2:
        if (heal_move_) {
            heal_move_(doc.x(), doc.y());
        }
        break;
    case 3:
        if (clone_move_) {
            clone_move_(doc.x(), doc.y());
        }
        break;
    default:
        break;
    }
}

}  // namespace compositor::appwin
