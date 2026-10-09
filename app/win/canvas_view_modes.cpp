#include <algorithm>
#include <utility>
#include <vector>

#include "canvas_view.hpp"

// The canvas mode and handler setters, split out so canvas_view.cpp stays within the soft size limit.

namespace compositor::appwin {
void CanvasView::set_move_mode(bool move) {
    move_mode_ = move;
    update_cursor();
}

void CanvasView::set_move_handler(std::function<void(double, double)> handler) {
    move_handler_ = std::move(handler);
}

void CanvasView::set_move_begin_handler(std::function<void()> handler) {
    move_begin_ = std::move(handler);
}

void CanvasView::set_paint_mode(bool paint) {
    paint_mode_ = paint;
    update_cursor();
}

void CanvasView::set_paint_handlers(std::function<void(double, double)> begin, std::function<void(double, double)> move,
                                    std::function<void()> end) {
    paint_begin_ = std::move(begin);
    paint_move_ = std::move(move);
    paint_end_ = std::move(end);
}

void CanvasView::set_select_mode(bool select) {
    select_mode_ = select;
    update_cursor();
}

void CanvasView::set_selection_handler(std::function<void(double, double, double, double)> handler) {
    selection_handler_ = std::move(handler);
}

void CanvasView::set_wand_mode(bool wand) {
    wand_mode_ = wand;
    update_cursor();
}

void CanvasView::set_wand_handler(std::function<void(double, double)> handler) {
    wand_handler_ = std::move(handler);
}

void CanvasView::set_lasso_mode(bool lasso) {
    lasso_mode_ = lasso;
    update_cursor();
}

void CanvasView::set_lasso_handler(std::function<void(const std::vector<QPointF>&)> handler) {
    lasso_handler_ = std::move(handler);
}

void CanvasView::set_polygon_mode(bool polygon) {
    polygon_mode_ = polygon;
    if (!polygon) {
        polygon_points_.clear();
        update();
    }
    update_cursor();
}

void CanvasView::set_eyedropper_mode(bool eyedropper) {
    eyedropper_mode_ = eyedropper;
    update_cursor();
}

void CanvasView::set_eyedropper_handler(std::function<void(double, double)> handler) {
    eyedropper_handler_ = std::move(handler);
}

void CanvasView::set_gradient_mode(bool gradient) {
    gradient_mode_ = gradient;
    update_cursor();
}

void CanvasView::set_gradient_handler(std::function<void(double, double, double, double)> handler) {
    gradient_handler_ = std::move(handler);
}

void CanvasView::set_heal_mode(bool heal) {
    heal_mode_ = heal;
    update_cursor();
}

void CanvasView::set_heal_handlers(std::function<void(double, double)> begin, std::function<void(double, double)> move,
                                   std::function<void()> end) {
    heal_begin_ = std::move(begin);
    heal_move_ = std::move(move);
    heal_end_ = std::move(end);
}

void CanvasView::set_clone_mode(bool clone) {
    clone_mode_ = clone;
    update_cursor();
}

void CanvasView::set_clone_handlers(std::function<void(double, double)> source,
                                    std::function<void(double, double)> begin, std::function<void(double, double)> move,
                                    std::function<void()> end) {
    clone_source_ = std::move(source);
    clone_begin_ = std::move(begin);
    clone_move_ = std::move(move);
    clone_end_ = std::move(end);
}

void CanvasView::set_crop_mode(bool crop) {
    crop_mode_ = crop;
    update_cursor();
}

void CanvasView::set_crop_handler(std::function<void(double, double, double, double)> handler) {
    crop_handler_ = std::move(handler);
}

void CanvasView::set_text_mode(bool text) {
    text_mode_ = text;
    update_cursor();
}

void CanvasView::set_text_handler(std::function<void(double, double)> handler) {
    text_handler_ = std::move(handler);
}

void CanvasView::set_brush_diameter(double diameter) {
    brush_diameter_ = std::max(0.0, diameter);
    update();
}

void CanvasView::set_rulers_visible(bool visible) {
    if (rulers_visible_ == visible) {
        return;
    }
    rulers_visible_ = visible;
    update();
}

void CanvasView::set_guide_handler(std::function<void(bool, double)> handler) {
    guide_handler_ = std::move(handler);
}

void CanvasView::set_image_changed_handler(std::function<void()> handler) {
    image_changed_handler_ = std::move(handler);
}

void CanvasView::set_pan(const QPointF& pan) {
    pan_ = pan;
    fit_ = false;
    update();
    if (view_changed_handler_) {
        view_changed_handler_();
    }
}

QSize CanvasView::viewport_size() const {
    return viewport_rect().size().toSize();
}

void CanvasView::set_view_changed_handler(std::function<void()> handler) {
    view_changed_handler_ = std::move(handler);
}

void CanvasView::notify_view_changed() {
    if (view_changed_handler_) {
        view_changed_handler_();
    }
}

void CanvasView::set_distort_mode(bool distort) {
    distort_mode_ = distort;
    if (!distort) {
        distorting_ = false;
        distort_corner_ = -1;
    }
    update_cursor();
}

void CanvasView::set_distort_quad(const std::vector<QPointF>& corners) {
    distort_quad_ = corners;
    update();
}

void CanvasView::set_distort_handler(std::function<void(const std::vector<QPointF>&)> handler) {
    distort_handler_ = std::move(handler);
}

}  // namespace compositor::appwin
