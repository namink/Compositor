#include <QColor>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <algorithm>
#include <cmath>
#include <vector>

#include "canvas_view.hpp"
#include "main_window.hpp"
#include "scrub_widgets.hpp"

// The canvas callbacks, split out so `main_window.cpp` stays within the size limit.

namespace compositor::appwin {
namespace {

[[nodiscard]] render::CombineMode combine_mode(int mode) {
    using render::CombineMode;
    switch (mode) {
    case 1:
        return CombineMode::add;
    case 2:
        return CombineMode::subtract;
    case 3:
        return CombineMode::intersect;
    default:
        return CombineMode::replace;
    }
}

}  // namespace

void MainWindow::refresh_distort_quad() {
    if (!distort_tool_ || selected_id_.isEmpty()) {
        return;
    }
    render::Quad quad;
    if (!session_->layer_quad(selected_id_.toStdString(), quad)) {
        return;
    }
    std::vector<QPointF> corners;
    corners.reserve(4);
    for (int i = 0; i < 4; ++i) {
        corners.emplace_back(quad.x[i], quad.y[i]);
    }
    canvas_->set_distort_quad(corners);
}

void MainWindow::wire_canvas() {
    connect(layers_, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem*, QTreeWidgetItem*) { on_layer_selected(); });
    canvas_->set_rulers_visible(shows_rulers_);
    canvas_->set_image_changed_handler([this] {
        refresh_history();
        refresh_navigator();
    });
    canvas_->set_view_changed_handler([this] { refresh_navigator(); });
    canvas_->set_text_handler([this](double x, double y) { text_tool_click(x, y); });
    canvas_->set_distort_handler([this](const std::vector<QPointF>& corners) {
        if (selected_id_.isEmpty() || corners.size() != 4) {
            return;
        }
        render::Quad quad;
        for (int i = 0; i < 4; ++i) {
            quad.x[i] = corners[static_cast<std::size_t>(i)].x();
            quad.y[i] = corners[static_cast<std::size_t>(i)].y();
        }
        session_->set_edit_name("Distort");
        QString error;
        if (!session_->apply_distort(selected_id_.toStdString(), quad, error)) {
            report(error);
            return;
        }
        refresh_layers();
        canvas_->updateImage(session_->image());
        refresh_distort_quad();
    });
    canvas_->set_guide_handler([this](bool horizontal, double position) {
        QString error;
        if (!session_->add_guide(horizontal, position, error)) {
            report(error);
            return;
        }
        refresh_overlays();
    });
    canvas_->set_move_begin_handler([this] {
        session_->begin_interaction();
        move_accum_x_ = 0.0;
        move_accum_y_ = 0.0;
        if (const model::ProjectLayerRecord* record = session_->layer(selected_id_.toStdString())) {
            move_base_ = record->transform;
        }
    });
    canvas_->set_move_handler([this](double dx, double dy) { apply_move_snap(dx, dy); });
    canvas_->set_paint_handlers(
        [this](double x, double y) {
            if (selected_id_.isEmpty()) {
                return;
            }
            QString error;
            if (blur_tool_ && blur_mode_ > 0) {
                if (!session_->warp_begin(selected_id_.toStdString(), x, y, blur_mode_ - 1, error)) {
                    report(error);
                }
                return;
            }
            if (!session_->stroke_begin(selected_id_.toStdString(), x, y, error)) {
                report(error);
                return;
            }
            canvas_->updateImage(session_->image());
        },
        [this](double x, double y) {
            if (blur_tool_ && blur_mode_ > 0) {
                session_->warp_move(x, y);
                canvas_->updateImage(session_->image());
                return;
            }
            QString error;
            if (session_->stroke_move(x, y, error)) {
                canvas_->updateImage(session_->image());
            }
        },
        [this] {
            if (blur_tool_ && blur_mode_ > 0) {
                session_->warp_end();
                canvas_->updateImage(session_->image());
                return;
            }
            session_->stroke_end();
        });
    canvas_->set_heal_handlers(
        [this](double x, double y) {
            if (selected_id_.isEmpty()) {
                return;
            }
            QString error;
            if (!session_->heal_begin(selected_id_.toStdString(), x, y, error)) {
                report(error);
            }
        },
        [this](double x, double y) { session_->heal_move(x, y); },
        [this] {
            session_->heal_end();
            canvas_->updateImage(session_->image());
        });
    canvas_->set_clone_handlers(
        [this](double x, double y) {
            if (selected_id_.isEmpty()) {
                return;
            }
            QString error;
            if (!session_->clone_set_source(selected_id_.toStdString(), x, y, error)) {
                report(error);
            } else {
                statusBar()->showMessage(QStringLiteral("Clone source set"), 2000);
            }
        },
        [this](double x, double y) {
            if (selected_id_.isEmpty()) {
                return;
            }
            QString error;
            if (!session_->clone_begin(selected_id_.toStdString(), x, y, error)) {
                report(error);
            }
        },
        [this](double x, double y) { session_->clone_move(x, y); },
        [this] {
            session_->clone_end();
            canvas_->updateImage(session_->image());
        });
    canvas_->set_crop_handler([this](double x, double y, double w, double h) {
        if (w < 1.0 || h < 1.0) {
            return;
        }
        session_->select_rect(render::DocRect{x, y, w, h}, render::CombineMode::replace);
        QString error;
        if (!session_->crop_to_selection(error)) {
            report(error);
            return;
        }
        refresh_layers();
        canvas_->set_selection_outline({});
        refresh_overlays();
        canvas_->updateImage(session_->image());
    });
    canvas_->set_selection_handler([this](double x, double y, double w, double h) {
        const render::DocRect rect{x, y, w, h};
        if (shape_mode_ != 0) {
            // The Shape tool adds a new layer whose pixels are the shape, keeping its style so it stays
            // editable, matching the macOS app's Shape tool.
            const int shape_width = std::max(1, static_cast<int>(std::lround(w)));
            const int shape_height = std::max(1, static_cast<int>(std::lround(h)));
            render::ShapeStyle style;
            style.kind = shape_mode_ == 2 ? render::ShapeKind::ellipse : render::ShapeKind::rectangle;
            style.red = brush_color_.redF();
            style.green = brush_color_.greenF();
            style.blue = brush_color_.blueF();
            style.corner_radius = shape_corner_radius_;
            QString error;
            if (!session_->add_shape_layer(style, std::floor(x), std::floor(y), shape_width, shape_height, error)) {
                report(error);
                return;
            }
            refresh_layers();
            canvas_->updateImage(session_->image());
            return;
        }
        const render::CombineMode mode = combine_mode(canvas_->selection_mode());
        if (ellipse_tool_) {
            session_->select_ellipse(rect, mode);
        } else {
            session_->select_rect(rect, mode);
        }
        refresh_selection_outline();
    });
    canvas_->set_gradient_handler([this](double sx, double sy, double ex, double ey) {
        if (selected_id_.isEmpty()) {
            return;
        }
        QString error;
        if (!session_->draw_gradient(selected_id_.toStdString(), brush_color_.redF(), brush_color_.greenF(),
                                     brush_color_.blueF(), 1.0, brush_color_.redF(), brush_color_.greenF(),
                                     brush_color_.blueF(), 0.0, sx, sy, ex, ey, false, error)) {
            report(error);
            return;
        }
        canvas_->updateImage(session_->image());
    });
    canvas_->set_wand_handler([this](double x, double y) {
        if (selected_id_.isEmpty()) {
            return;
        }
        const int tolerance = wand_tolerance_ != nullptr ? static_cast<int>(wand_tolerance_->value()) : 32;
        QString error;
        if (!session_->select_wand(selected_id_.toStdString(), x, y, tolerance, true,
                                   combine_mode(canvas_->selection_mode()), error)) {
            report(error);
            return;
        }
        refresh_selection_outline();
    });
    canvas_->set_lasso_handler([this](const std::vector<QPointF>& points) {
        std::vector<render::Point> document;
        document.reserve(points.size());
        for (const QPointF& point : points) {
            document.push_back(render::Point{point.x(), point.y()});
        }
        session_->select_lasso(document, combine_mode(canvas_->selection_mode()));
        refresh_selection_outline();
    });
    canvas_->set_eyedropper_handler([this](double x, double y) {
        const QColor color = session_->sample_color(x, y);
        if (!color.isValid()) {
            return;
        }
        brush_color_ = color;
        if (color_button_ != nullptr) {
            color_button_->setStyleSheet(QStringLiteral("background-color: %1").arg(brush_color_.name()));
        }
        on_brush_changed();
        statusBar()->showMessage(QStringLiteral("Picked %1").arg(color.name()), 3000);
    });
}

void MainWindow::apply_move_snap(double dx, double dy) {
    if (selected_id_.isEmpty()) {
        return;
    }
    const model::ProjectManifest* manifest = session_->manifest();
    if (manifest == nullptr) {
        return;
    }
    move_accum_x_ += dx;
    move_accum_y_ += dy;
    const double size_w = move_base_.width;
    const double size_h = move_base_.height;
    const double tolerance = 10.0 / std::max(0.0001, canvas_->scale());

    std::vector<double> xs{0.0, static_cast<double>(manifest->width)};
    std::vector<double> ys{0.0, static_cast<double>(manifest->height)};
    if (const std::vector<model::CanvasGuide>* guides = session_->guides()) {
        for (const model::CanvasGuide& guide : *guides) {
            if (guide.axis == model::CanvasGuide::Axis::horizontal) {
                ys.push_back(guide.position);
            } else {
                xs.push_back(guide.position);
            }
        }
    }
    if (shows_grid_ && grid_spacing_ > 0 && grid_subdivisions_ > 0) {
        const double step = static_cast<double>(grid_spacing_) / grid_subdivisions_;
        for (double x = 0.0; x <= manifest->width; x += step) {
            xs.push_back(x);
        }
        for (double y = 0.0; y <= manifest->height; y += step) {
            ys.push_back(y);
        }
    }
    const std::string selected = selected_id_.toStdString();
    for (const model::ProjectLayerRecord& layer : manifest->layers) {
        if (layer.id == selected || layer.is_group_layer()) {
            continue;
        }
        xs.push_back(layer.transform.origin_x);
        xs.push_back(layer.transform.origin_x + layer.transform.width / 2.0);
        xs.push_back(layer.transform.origin_x + layer.transform.width);
        ys.push_back(layer.transform.origin_y);
        ys.push_back(layer.transform.origin_y + layer.transform.height / 2.0);
        ys.push_back(layer.transform.origin_y + layer.transform.height);
    }
    const auto snap = [tolerance](double origin, double size, const std::vector<double>& targets) {
        const double anchors[3] = {origin, origin + size / 2.0, origin + size};
        double best = 0.0;
        bool found = false;
        for (double anchor : anchors) {
            for (double target : targets) {
                const double move = target - anchor;
                if (std::abs(move) <= tolerance && (!found || std::abs(move) < std::abs(best))) {
                    best = move;
                    found = true;
                }
            }
        }
        return origin + (found ? best : 0.0);
    };
    const double nx = snap(move_base_.origin_x + move_accum_x_, size_w, xs);
    const double ny = snap(move_base_.origin_y + move_accum_y_, size_h, ys);
    QString error;
    if (session_->set_layer_origin(selected, nx, ny, error)) {
        canvas_->updateImage(session_->image());
    }
}

}  // namespace compositor::appwin
