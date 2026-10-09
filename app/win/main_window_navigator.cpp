#include <QColor>
#include <QDockWidget>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QWidget>
#include <algorithm>

#include "canvas_view.hpp"
#include "main_window.hpp"

// The navigator (macOS's NavigationToolHeader / CanvasThumbnail): a thumbnail of the document with a
// rectangle for what the canvas shows, draggable to pan.

namespace compositor::appwin {

class NavigatorWidget : public QWidget {
public:
    explicit NavigatorWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(200, 150);
        setMouseTracking(true);
    }

    void set_canvas(CanvasView* canvas) { canvas_ = canvas; }

    void set_image(const QImage& image) {
        image_ = image;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(35, 35, 35));
        if (image_.isNull() || canvas_ == nullptr) {
            return;
        }
        const QRectF target = fitted();
        painter.drawImage(target, image_);
        const QSize viewport = canvas_->viewport_size();
        const QSize document = canvas_->document_size();
        if (viewport.width() <= 0 || document.width() <= 0) {
            return;
        }
        const double scale = canvas_->scale();
        const QSizeF shown(document.width() * scale, document.height() * scale);
        const QSizeF visible(viewport.width() / scale, viewport.height() / scale);
        const QPointF center(document.width() / 2.0 - canvas_->pan().x() / scale,
                             document.height() / 2.0 - canvas_->pan().y() / scale);
        const double k = target.width() / document.width();
        const QPointF top_left = center - QPointF(visible.width() / 2.0, visible.height() / 2.0);
        const QRectF frame(target.left() + top_left.x() * k, target.top() + top_left.y() * k, visible.width() * k,
                           visible.height() * k);
        QPen pen(QColor(42, 130, 218));
        pen.setWidth(2);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(frame);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            pan_to(event->position());
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if ((event->buttons() & Qt::LeftButton) != 0) {
            pan_to(event->position());
        }
    }

private:
    [[nodiscard]] QRectF fitted() const {
        const double ratio =
            std::min(static_cast<double>(width()) / image_.width(), static_cast<double>(height()) / image_.height());
        const QSizeF shown(image_.width() * ratio, image_.height() * ratio);
        return QRectF((width() - shown.width()) / 2.0, (height() - shown.height()) / 2.0, shown.width(),
                      shown.height());
    }

    void pan_to(const QPointF& position) {
        if (canvas_ == nullptr || image_.isNull()) {
            return;
        }
        const QRectF target = fitted();
        const double k = image_.width() / target.width();
        const double document_x = (position.x() - target.left()) * k;
        const double document_y = (position.y() - target.top()) * k;
        const double scale = canvas_->scale();
        const QSize document = canvas_->document_size();
        // Center the view on that document point: pan = scale * (docSize/2 - point).
        const QPointF pan(scale * (document.width() / 2.0 - document_x),
                          scale * (document.height() / 2.0 - document_y));
        canvas_->set_pan(pan);
    }

    QImage image_;
    CanvasView* canvas_ = nullptr;
};

void MainWindow::build_navigator_dock() {
    auto* dock = new QDockWidget(QStringLiteral("Navigator"), this);
    navigator_ = new NavigatorWidget(dock);
    navigator_->set_canvas(canvas_);
    dock->setWidget(navigator_);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    navigator_dock_ = dock;
}

void MainWindow::refresh_navigator() {
    if (navigator_ == nullptr) {
        return;
    }
    navigator_->set_image(session_->is_open() ? session_->image() : QImage());
}

}  // namespace compositor::appwin
