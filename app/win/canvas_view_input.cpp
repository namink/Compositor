#include <QGuiApplication>
#include <QMouseEvent>
#include <QRectF>
#include <QTimer>
#include <cmath>

#include "canvas_view.hpp"

// Mouse input for the canvas, split out so `canvas_view.cpp` stays within the soft size limit.

namespace compositor::appwin {

void CanvasView::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || image_.isNull()) {
        return;
    }
    last_mouse_ = event->position();
    if (rulers_visible_ && guide_handler_) {
        const QRectF viewport = viewport_rect();
        const QPointF position = event->position();
        if (position.y() < viewport.top() && position.x() >= viewport.left()) {
            dragging_guide_ = true;
            guide_horizontal_ = true;
            guide_position_ = to_document(position).y();
            update();
            return;
        }
        if (position.x() < viewport.left() && position.y() >= viewport.top()) {
            dragging_guide_ = true;
            guide_horizontal_ = false;
            guide_position_ = to_document(position).x();
            update();
            return;
        }
    }
    if (distort_mode_ && distort_quad_.size() == 4) {
        const QPointF position = event->position();
        int nearest = -1;
        double best = 12.0;
        for (int i = 0; i < 4; ++i) {
            const QPointF corner = to_widget(distort_quad_[static_cast<std::size_t>(i)]);
            const double distance = std::hypot(corner.x() - position.x(), corner.y() - position.y());
            if (distance < best) {
                best = distance;
                nearest = i;
            }
        }
        if (nearest >= 0) {
            distorting_ = true;
            distort_corner_ = nearest;
            return;
        }
    }
    const Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
    const bool shift = modifiers.testFlag(Qt::ShiftModifier);
    const bool alt = modifiers.testFlag(Qt::AltModifier);
    selection_mode_ = shift && alt ? 3 : shift ? 1 : alt ? 2 : 0;

    if (paint_mode_ && paint_begin_) {
        painting_ = true;
        const QPointF doc = to_document(event->position());
        paint_begin_(doc.x(), doc.y());
    } else if (eyedropper_mode_ && eyedropper_handler_) {
        const QPointF doc = to_document(event->position());
        eyedropper_handler_(doc.x(), doc.y());
    } else if (gradient_mode_ && gradient_handler_) {
        gradienting_ = true;
        gradient_start_ = to_document(event->position());
        gradient_end_ = gradient_start_;
        update();
    } else if (heal_mode_ && heal_begin_) {
        healing_ = true;
        const QPointF doc = to_document(event->position());
        heal_begin_(doc.x(), doc.y());
    } else if (clone_mode_ && clone_begin_) {
        const QPointF doc = to_document(event->position());
        if (alt) {
            if (clone_source_) {
                clone_source_(doc.x(), doc.y());
            }
        } else {
            cloning_ = true;
            clone_begin_(doc.x(), doc.y());
        }
    } else if (text_mode_ && text_handler_) {
        const QPointF doc = to_document(event->position());
        text_handler_(doc.x(), doc.y());
    } else if (crop_mode_ && crop_handler_) {
        croping_ = true;
        crop_anchor_ = to_document(event->position());
        crop_rect_ = QRectF(crop_anchor_, crop_anchor_);
        update();
    } else if (wand_mode_ && wand_handler_) {
        const QPointF doc = to_document(event->position());
        wand_handler_(doc.x(), doc.y());
    } else if (polygon_mode_) {
        polygon_points_.push_back(to_document(event->position()));
        update();
    } else if (lasso_mode_ && lasso_handler_) {
        lassoing_ = true;
        lasso_points_.assign(1, to_document(event->position()));
        update();
    } else if (select_mode_ && selection_handler_) {
        selecting_ = true;
        select_anchor_ = to_document(event->position());
        update();
    } else if (move_mode_ && move_handler_) {
        moving_ = true;
        if (move_begin_) {
            move_begin_();
        }
    } else {
        panning_ = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

void CanvasView::mouseMoveEvent(QMouseEvent* event) {
    if (distorting_) {
        distort_quad_[static_cast<std::size_t>(distort_corner_)] = to_document(event->position());
        update();
        return;
    }
    if (dragging_guide_) {
        const QPointF document = to_document(event->position());
        guide_position_ = guide_horizontal_ ? document.y() : document.x();
        update();
        return;
    }
    if (!painting_ && !healing_ && !cloning_ && !croping_ && !gradienting_ && !lassoing_ && !selecting_ && !moving_ &&
        !panning_ && !distorting_) {
        // Plain hover: track the pointer so the brush cursor circle follows it.
        hover_pos_ = event->position();
        hovering_ = true;
        update();
        return;
    }
    if (painting_ || healing_ || cloning_) {
        stroke_pending_mode_ = painting_ ? 1 : (healing_ ? 2 : 3);
        stroke_pending_pos_ = to_document(event->position());
        stroke_pending_ = true;
        if (stroke_timer_ != nullptr && !stroke_timer_->isActive()) {
            stroke_timer_->start();
        }
        return;
    }
    if (croping_) {
        crop_rect_ = QRectF(crop_anchor_, to_document(event->position())).normalized();
        update();
        return;
    }
    if (gradienting_) {
        gradient_end_ = to_document(event->position());
        update();
        return;
    }
    if (lassoing_) {
        lasso_points_.push_back(to_document(event->position()));
        update();
        return;
    }
    if (selecting_) {
        const QPointF current = to_document(event->position());
        const QRectF rect = QRectF(select_anchor_, current).normalized();
        selection_handler_(rect.x(), rect.y(), rect.width(), rect.height());
        return;
    }
    if (moving_) {
        const QPointF delta = event->position() - last_mouse_;
        last_mouse_ = event->position();
        const double scale = current_scale();
        if (scale > 0.0) {
            move_handler_(delta.x() / scale, delta.y() / scale);
        }
        return;
    }
    if (panning_) {
        pan_ += event->position() - last_mouse_;
        last_mouse_ = event->position();
        update();
        notify_view_changed();
    }
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (polygon_mode_ && event->button() == Qt::LeftButton) {
        if (lasso_handler_ && polygon_points_.size() >= 3) {
            lasso_handler_(polygon_points_);
        }
        polygon_points_.clear();
        update();
        return;
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (distorting_) {
        distorting_ = false;
        distort_corner_ = -1;
        if (distort_handler_) {
            distort_handler_(distort_quad_);
        }
        return;
    }
    if (dragging_guide_) {
        dragging_guide_ = false;
        const QRectF viewport = viewport_rect();
        const QPointF position = event->position();
        // Dropping back on the ruler removes nothing new; dropping in the canvas creates the guide.
        const bool over_ruler = position.x() < viewport.left() || position.y() < viewport.top();
        if (!over_ruler && guide_handler_) {
            guide_handler_(guide_horizontal_, guide_position_);
        }
        update();
        return;
    }
    if (stroke_pending_) {
        flush_stroke_move();  // paint the last position before ending the stroke
    }
    if (painting_) {
        painting_ = false;
        if (paint_end_) {
            paint_end_();
        }
    }
    if (lassoing_) {
        lassoing_ = false;
        if (lasso_handler_ && lasso_points_.size() >= 3) {
            lasso_handler_(lasso_points_);
        }
        lasso_points_.clear();
        update();
    }
    if (selecting_) {
        selecting_ = false;
        if (selection_handler_) {
            const QPointF current = to_document(event->position());
            const QRectF rect = QRectF(select_anchor_, current).normalized();
            selection_handler_(rect.x(), rect.y(), rect.width(), rect.height());
        }
    }
    if (healing_) {
        healing_ = false;
        if (heal_end_) {
            heal_end_();
        }
    }
    if (cloning_) {
        cloning_ = false;
        if (clone_end_) {
            clone_end_();
        }
    }
    if (croping_) {
        croping_ = false;
        if (crop_handler_) {
            crop_handler_(crop_rect_.x(), crop_rect_.y(), crop_rect_.width(), crop_rect_.height());
        }
        crop_rect_ = QRectF();
        update();
    }
    if (gradienting_) {
        gradienting_ = false;
        if (gradient_handler_) {
            gradient_handler_(gradient_start_.x(), gradient_start_.y(), gradient_end_.x(), gradient_end_.y());
        }
        update();
    }
    moving_ = false;
    panning_ = false;
    update_cursor();
}

}  // namespace compositor::appwin
