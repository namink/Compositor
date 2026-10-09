#include <QInputDialog>
#include <QStringList>
#include <string>

#include "adjustment_dialog.hpp"
#include "camera_raw_color_dialog.hpp"
#include "canvas_view.hpp"
#include "curves_dialog.hpp"
#include "hsv_dialog.hpp"
#include "levels_dialog.hpp"
#include "main_window.hpp"
#include "tool_dock.hpp"

// Adjustment-layer, filter, mask and canvas actions, split so `main_window_edit.cpp` stays in size.

namespace compositor::appwin {

void MainWindow::refresh_overlays() {
    std::vector<std::pair<int, double>> guides;
    const std::vector<model::CanvasGuide>* source = session_->guides();
    if (source != nullptr) {
        for (const model::CanvasGuide& guide : *source) {
            guides.emplace_back(guide.axis == model::CanvasGuide::Axis::horizontal ? 0 : 1, guide.position);
        }
    }
    canvas_->set_guides(std::move(guides));
    canvas_->set_grid(shows_grid_, grid_spacing_, grid_subdivisions_);
}

void MainWindow::toggle_grid(bool visible) {
    shows_grid_ = visible;
    refresh_overlays();
    save_tool_defaults();
}

void MainWindow::toggle_rulers(bool visible) {
    shows_rulers_ = visible;
    canvas_->set_rulers_visible(visible);
    save_tool_defaults();
}

void MainWindow::grid_settings() {
    bool ok = false;
    const int spacing = QInputDialog::getInt(this, QStringLiteral("Grid Settings"), QStringLiteral("Spacing (px)"),
                                             grid_spacing_, 2, 4096, 1, &ok);
    if (!ok) {
        return;
    }
    const int subdivisions = QInputDialog::getInt(this, QStringLiteral("Grid Settings"), QStringLiteral("Subdivisions"),
                                                  grid_subdivisions_, 1, 64, 1, &ok);
    if (!ok) {
        return;
    }
    grid_spacing_ = spacing;
    grid_subdivisions_ = subdivisions;
    refresh_overlays();
    save_tool_defaults();
}

void MainWindow::new_guide() {
    if (!session_->is_open()) {
        return;
    }
    bool ok = false;
    const QStringList orientations{QStringLiteral("Horizontal"), QStringLiteral("Vertical")};
    const QString orientation = QInputDialog::getItem(this, QStringLiteral("New Guide"), QStringLiteral("Orientation"),
                                                      orientations, 0, false, &ok);
    if (!ok) {
        return;
    }
    const double position = QInputDialog::getDouble(this, QStringLiteral("New Guide"), QStringLiteral("Position (px)"),
                                                    0.0, -1000000.0, 1000000.0, 1, &ok);
    if (!ok) {
        return;
    }
    QString error;
    if (!session_->add_guide(orientation == orientations[0], position, error)) {
        report(error);
        return;
    }
    refresh_overlays();
}

void MainWindow::clear_guides() {
    QString error;
    if (!session_->clear_guides(error)) {
        report(error);
        return;
    }
    refresh_overlays();
}

void MainWindow::canvas_size_menu() {
    const model::ProjectManifest* manifest = session_->manifest();
    if (manifest == nullptr) {
        return;
    }
    bool ok = false;
    const int width = QInputDialog::getInt(this, QStringLiteral("Canvas Size"), QStringLiteral("Width"),
                                           manifest->width, 1, 30000, 1, &ok);
    if (!ok) {
        return;
    }
    const int height = QInputDialog::getInt(this, QStringLiteral("Canvas Size"), QStringLiteral("Height"),
                                            manifest->height, 1, 30000, 1, &ok);
    if (!ok) {
        return;
    }
    const QStringList anchors{QStringLiteral("Top-Left"),    QStringLiteral("Top"),    QStringLiteral("Top-Right"),
                              QStringLiteral("Left"),        QStringLiteral("Center"), QStringLiteral("Right"),
                              QStringLiteral("Bottom-Left"), QStringLiteral("Bottom"), QStringLiteral("Bottom-Right")};
    const QString anchor =
        QInputDialog::getItem(this, QStringLiteral("Canvas Size"), QStringLiteral("Anchor"), anchors, 4, false, &ok);
    if (!ok) {
        return;
    }
    QString error;
    if (!session_->canvas_size(width, height, static_cast<int>(anchors.indexOf(anchor)), error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::image_size_menu() {
    const model::ProjectManifest* manifest = session_->manifest();
    if (manifest == nullptr) {
        return;
    }
    bool ok = false;
    const int width = QInputDialog::getInt(this, QStringLiteral("Image Size"), QStringLiteral("Width"), manifest->width,
                                           1, 30000, 1, &ok);
    if (!ok) {
        return;
    }
    const int height = QInputDialog::getInt(this, QStringLiteral("Image Size"), QStringLiteral("Height"),
                                            manifest->height, 1, 30000, 1, &ok);
    if (!ok) {
        return;
    }
    QString error;
    if (!session_->image_size(width, height, error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::trim_canvas() {
    session_->set_edit_name("Trim");
    QString error;
    if (!session_->trim(error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::auto_levels(int mode) {
    session_->set_edit_name("Auto Levels");
    QString error;
    if (!session_->auto_levels(mode, error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::new_adjustment_layer(const QString& kind) {
    session_->set_edit_name("Adjustment: " + kind.toStdString());
    QString error;
    if (!session_->add_adjustment_layer(selected_id_.toStdString(), kind.toStdString(), error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::filter_layer(const QString& kind) {
    if (selected_id_.isEmpty()) {
        return;
    }
    session_->set_edit_name("Filter: " + kind.toStdString());
    nlohmann::json value = {{"kind", kind.toStdString()}};
    if (kind == QStringLiteral("Dither")) {
        if (!prompt_dither(this, value)) {
            return;
        }
    } else if (kind == QStringLiteral("Scanlines")) {
        if (!prompt_scanlines(this, value)) {
            return;
        }
    } else if (kind == QStringLiteral("Curves")) {
        if (!prompt_curves(this, value)) {
            return;
        }
    } else if (kind == QStringLiteral("Levels")) {
        if (!prompt_levels(this, session_->histogram(), value)) {
            return;
        }
    } else if (kind == QStringLiteral("Hue/Saturation")) {
        if (!prompt_hsv(this, value)) {
            return;
        }
    } else if (!prompt_adjustment(this, kind, QStringLiteral("Filter: %1").arg(kind), value)) {
        return;
    }
    QString error;
    if (!session_->apply_filter(selected_id_.toStdString(), value, error)) {
        report(error);
        return;
    }
    last_filter_ = value;
    has_last_filter_ = true;
    canvas_->updateImage(session_->image());
}

void MainWindow::last_filter() {
    if (selected_id_.isEmpty() || !has_last_filter_) {
        return;
    }
    session_->set_edit_name("Last Filter");
    QString error;
    if (!session_->apply_filter(selected_id_.toStdString(), last_filter_, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::camera_raw_color() {
    if (selected_id_.isEmpty()) {
        return;
    }
    if (tool_dock_ == nullptr) {
        tool_dock_ = new ToolDock(QStringLiteral("Camera Raw Color"), this);
        addDockWidget(Qt::RightDockWidgetArea, tool_dock_);
        tool_dock_->hide();
    }
    const std::string id = selected_id_.toStdString();
    editing_filter_ = nlohmann::json{{"kind", "Camera Raw"}};
    QString error;
    if (!session_->begin_filter_preview(id, error)) {
        report(error);
        return;
    }
    nlohmann::json& style = editing_filter_;
    QWidget* panel = build_camera_raw_color_panel(nullptr, style, [this, id] {
        QString preview_error;
        session_->update_filter_preview(editing_filter_, preview_error);
        canvas_->updateImage(session_->image());
    });
    session_->set_edit_name("Camera Raw Color");
    tool_dock_->show_content(
        QStringLiteral("Camera Raw Color"), panel,
        [this, id] {
            QString commit_error;
            session_->commit_filter_preview(commit_error);
            canvas_->updateImage(session_->image());
        },
        [this] {
            session_->cancel_filter_preview();
            canvas_->updateImage(session_->image());
        });
}

void MainWindow::edit_adjustment() {
    if (selected_id_.isEmpty()) {
        return;
    }
    const model::ProjectLayerRecord* record = session_->layer(selected_id_.toStdString());
    if (record == nullptr || !record->adjustment) {
        report(QStringLiteral("Select an adjustment layer to edit."));
        return;
    }
    session_->set_edit_name("Edit Adjustment");
    nlohmann::json value = *record->adjustment;
    const QString kind = QString::fromStdString(value.value("kind", std::string("Invert")));
    const bool edited = kind == QStringLiteral("Curves")   ? prompt_curves(this, value)
                        : kind == QStringLiteral("Levels") ? prompt_levels(this, session_->histogram(), value)
                        : kind == QStringLiteral("Hue/Saturation")
                            ? prompt_hsv(this, value)
                            : prompt_adjustment(this, kind, QStringLiteral("Adjustment: %1").arg(kind), value);
    if (!edited) {
        return;
    }
    QString error;
    if (!session_->update_adjustment(selected_id_.toStdString(), value, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::add_mask(bool from_selection, bool invert) {
    if (selected_id_.isEmpty()) {
        return;
    }
    session_->set_edit_name("Add Mask");
    QString error;
    if (!session_->add_layer_mask(selected_id_.toStdString(), from_selection, invert, error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::remove_mask() {
    if (selected_id_.isEmpty()) {
        return;
    }
    session_->set_edit_name("Delete Mask");
    QString error;
    if (!session_->remove_layer_mask(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::invert_mask() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->invert_layer_mask(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::toggle_mask() {
    if (selected_id_.isEmpty()) {
        return;
    }
    session_->set_edit_name("Toggle Mask");
    QString error;
    if (!session_->toggle_layer_mask(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::apply_mask() {
    if (selected_id_.isEmpty()) {
        return;
    }
    session_->set_edit_name("Apply Mask");
    QString error;
    if (!session_->apply_layer_mask(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::toggle_clipping() {
    if (selected_id_.isEmpty()) {
        return;
    }
    session_->set_edit_name("Clipping Mask");
    QString error;
    if (!session_->toggle_clipping_mask(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

}  // namespace compositor::appwin
