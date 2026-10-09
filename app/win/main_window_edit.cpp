#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QPushButton>
#include <QStatusBar>
#include <cstdint>
#include <string>

#include "canvas_view.hpp"
#include "color_range_dialog.hpp"
#include "color_swatch.hpp"
#include "main_window.hpp"
// Editing actions — undo/redo, selections, clipboard, export — split out so `main_window.cpp`
// stays within the soft size limit. Layer-panel actions live in `main_window_layer_ops.cpp`.

namespace compositor::appwin {

void MainWindow::undo() {
    QString error;
    if (!session_->undo(error)) {
        statusBar()->showMessage(QStringLiteral("Nothing to undo."), 3000);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::redo() {
    QString error;
    if (!session_->redo(error)) {
        statusBar()->showMessage(QStringLiteral("Nothing to redo."), 3000);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::delete_selection() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->delete_selection(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::fill_selection() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->fill_selection(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::content_aware_fill() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->content_aware_fill(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::float_selection() {
    QString error;
    if (!session_->float_selection(error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::select_all() {
    session_->select_all();
    refresh_selection_outline();
}

void MainWindow::deselect() {
    session_->clear_selection();
    canvas_->set_selection_outline({});
}

void MainWindow::invert_selection() {
    session_->invert_selection();
    refresh_selection_outline();
}

void MainWindow::load_layer_selection() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->load_layer_selection(selected_id_.toStdString(), render::CombineMode::replace, error)) {
        report(error);
        return;
    }
    refresh_selection_outline();
}

void MainWindow::load_mask_selection() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->load_mask_selection(selected_id_.toStdString(), render::CombineMode::replace, error)) {
        report(error);
        return;
    }
    refresh_selection_outline();
}

void MainWindow::color_range() {
    if (!session_->is_open()) {
        return;
    }
    QColor color = brush_color_;
    int fuzziness = 40;
    bool invert = false;
    if (!prompt_color_range(this, color, fuzziness, invert)) {
        return;
    }
    session_->set_edit_name("Color Range");
    QString error;
    if (!session_->color_range_select(static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
                                      static_cast<std::uint8_t>(color.blue()), fuzziness, invert,
                                      render::CombineMode::replace, error)) {
        report(error);
        return;
    }
    refresh_selection_outline();
}

void MainWindow::refine_edge() {
    if (!session_->is_open()) {
        return;
    }
    bool ok = false;
    const int radius =
        QInputDialog::getInt(this, QStringLiteral("Refine Edge"), QStringLiteral("Radius (px)"), 8, 1, 100, 1, &ok);
    if (!ok) {
        return;
    }
    QString error;
    if (!session_->refine_selection_edges(radius, error)) {
        report(error);
        return;
    }
    refresh_selection_outline();
}

void MainWindow::grow_selection() {
    session_->expand_selection(2);
    refresh_selection_outline();
}

void MainWindow::shrink_selection() {
    session_->contract_selection(2);
    refresh_selection_outline();
}

void MainWindow::feather_selection() {
    session_->feather_selection(4.0);
    refresh_selection_outline();
}

void MainWindow::crop_to_selection() {
    QString error;
    if (!session_->crop_to_selection(error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->set_selection_outline({});
    canvas_->updateImage(session_->image());
}

void MainWindow::refresh_selection_outline() {
    const std::vector<std::vector<render::Point>> loops = session_->selection_outline();
    std::vector<std::vector<QPointF>> polygon_loops;
    polygon_loops.reserve(loops.size());
    for (const std::vector<render::Point>& loop : loops) {
        std::vector<QPointF> path;
        path.reserve(loop.size());
        for (const render::Point& point : loop) {
            path.emplace_back(point.x, point.y);
        }
        polygon_loops.push_back(std::move(path));
    }
    canvas_->set_selection_outline(std::move(polygon_loops));
}

void MainWindow::export_jpeg_menu() {
    if (!session_->is_open()) {
        return;
    }
    bool ok = false;
    const int quality =
        QInputDialog::getInt(this, QStringLiteral("Export JPEG"), QStringLiteral("Quality"), 92, 1, 100, 1, &ok);
    if (!ok) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export JPEG"), QStringLiteral("export.jpg"),
                                                      QStringLiteral("JPEG image (*.jpg *.jpeg);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->export_jpeg(path, quality, error)) {
        report(error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Exported %1").arg(path), 4000);
}

void MainWindow::export_psd_menu() {
    if (!session_->is_open()) {
        return;
    }
    const QString path =
        QFileDialog::getSaveFileName(this, QStringLiteral("Export Photoshop"), QStringLiteral("export.psd"),
                                     QStringLiteral("Photoshop (*.psd);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->export_psd(path, error)) {
        report(error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Exported %1").arg(path), 4000);
}

void MainWindow::export_pdf_menu() {
    if (!session_->is_open()) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export PDF"), QStringLiteral("export.pdf"),
                                                      QStringLiteral("PDF (*.pdf);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->export_pdf(path, error)) {
        report(error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Exported %1").arg(path), 4000);
}

void MainWindow::copy_merged() {
    if (!session_->is_open()) {
        return;
    }
    QString error;
    if (!session_->copy_merged(error)) {
        report(error);
        return;
    }
    const QImage copied = session_->pixel_clipboard_image();
    if (!copied.isNull()) {
        QApplication::clipboard()->setImage(copied);
    }
    statusBar()->showMessage(QStringLiteral("Copied merged image."), 3000);
}

void MainWindow::copy_selection() {
    if (!session_->is_open()) {
        return;
    }
    QString error;
    if (!session_->copy_selection(error)) {
        report(error);
        return;
    }
    const QImage copied = session_->pixel_clipboard_image();
    if (!copied.isNull()) {
        QApplication::clipboard()->setImage(copied);
    }
    statusBar()->showMessage(QStringLiteral("Copied."), 3000);
}

void MainWindow::cut_selection() {
    if (!session_->is_open()) {
        return;
    }
    QString error;
    if (!session_->cut_selection(error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
    statusBar()->showMessage(QStringLiteral("Cut."), 3000);
}

void MainWindow::paste() {
    QString error;
    if (!session_->paste(error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
    statusBar()->showMessage(QStringLiteral("Pasted."), 3000);
}

void MainWindow::layer_via_copy() {
    QString error;
    if (!session_->layer_via_copy(error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::choose_brush_color() {
    const QColor chosen = QColorDialog::getColor(brush_color_, this, QStringLiteral("Brush color"));
    if (!chosen.isValid()) {
        return;
    }
    brush_color_ = chosen;
    if (color_button_ != nullptr) {
        color_button_->setStyleSheet(QStringLiteral("background-color: %1").arg(brush_color_.name()));
    }
    if (color_swatch_ != nullptr) {
        color_swatch_->set_foreground(brush_color_);
    }
    on_brush_changed();
}

}  // namespace compositor::appwin
