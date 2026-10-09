#include <QColor>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QStatusBar>
#include <QTreeWidgetItem>
#include <memory>
#include <string>

#include "canvas_view.hpp"
#include "layer_effects_dialog.hpp"
#include "main_window.hpp"
#include "shape_edit_dialog.hpp"
#include "text_dialog.hpp"
#include "text_runs_dialog.hpp"
// Layer-panel actions (create, reorder, rename, transform, visibility, blend), split out so
// `main_window_edit.cpp` stays within the soft size limit.

namespace compositor::appwin {

void MainWindow::add_layer() {
    session_->set_edit_name("New Layer");
    QString error;
    if (!session_->add_blank_layer(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::duplicate_layer() {
    if (selected_id_.isEmpty()) {
        return;
    }
    session_->set_edit_name("Duplicate Layer");
    QString error;
    if (!session_->duplicate_layer(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::delete_layer() {
    if (selected_id_.isEmpty()) {
        return;
    }
    session_->set_edit_name("Delete Layer");
    QString error;
    if (!session_->delete_layer(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::flip_horizontal() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->flip_layer(selected_id_.toStdString(), true, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::flip_vertical() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->flip_layer(selected_id_.toStdString(), false, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::rotate_cw() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->rotate_layer_90(selected_id_.toStdString(), true, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::rotate_ccw() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->rotate_layer_90(selected_id_.toStdString(), false, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::reset_transform() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->reset_transform(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::merge_layers() {
    session_->set_edit_name("Merge");
    QString error;
    if (!session_->merge_layers(error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::flatten() {
    session_->set_edit_name("Flatten");
    QString error;
    if (!session_->flatten(error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::new_folder() {
    session_->set_edit_name("New Folder");
    QString error;
    if (!session_->add_folder(selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::rename_layer() {
    if (selected_id_.isEmpty()) {
        return;
    }
    const model::ProjectLayerRecord* record = session_->layer(selected_id_.toStdString());
    const QString current = record != nullptr ? QString::fromStdString(record->name) : QString();
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Rename Layer"), QStringLiteral("Name"),
                                               QLineEdit::Normal, current, &ok);
    if (!ok) {
        return;
    }
    QString error;
    if (!session_->rename_layer(selected_id_.toStdString(), name, error)) {
        report(error);
        return;
    }
    refresh_layers();
}

void MainWindow::bring_forward() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->move_layer_order(selected_id_.toStdString(), 1, error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::send_backward() {
    if (selected_id_.isEmpty()) {
        return;
    }
    QString error;
    if (!session_->move_layer_order(selected_id_.toStdString(), -1, error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::flip_canvas_horizontal() {
    QString error;
    if (!session_->flip_canvas(true, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::flip_canvas_vertical() {
    QString error;
    if (!session_->flip_canvas(false, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::rotate_canvas_cw() {
    QString error;
    if (!session_->rotate_canvas(true, error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::rotate_canvas_ccw() {
    QString error;
    if (!session_->rotate_canvas(false, error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::layer_effects() {
    if (selected_id_.isEmpty()) {
        return;
    }
    const model::ProjectLayerRecord* record = session_->layer(selected_id_.toStdString());
    if (record == nullptr || record->is_group_layer() || record->adjustment) {
        report(QStringLiteral("Select a pixel layer to add effects to."));
        return;
    }
    nlohmann::json effects = record->effects ? *record->effects : nlohmann::json::object();
    if (!prompt_layer_effects(this, effects)) {
        return;
    }
    session_->set_edit_name("Layer Effects");
    QString error;
    if (!session_->set_effects(selected_id_.toStdString(), effects, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::edit_shape() {
    if (selected_id_.isEmpty()) {
        return;
    }
    const model::ProjectLayerRecord* record = session_->layer(selected_id_.toStdString());
    if (record == nullptr || !record->shape) {
        report(QStringLiteral("Select a shape layer to edit."));
        return;
    }
    const nlohmann::json& shape = *record->shape;
    render::ShapeStyle style;
    const std::string kind = shape.value("kind", std::string("Rectangle"));
    style.kind = kind == "Ellipse" ? render::ShapeKind::ellipse
                 : kind == "Line"  ? render::ShapeKind::line
                                   : render::ShapeKind::rectangle;
    style.red = static_cast<float>(shape.value("red", 0.0));
    style.green = static_cast<float>(shape.value("green", 0.0));
    style.blue = static_cast<float>(shape.value("blue", 0.0));
    style.corner_radius = shape.value("cornerRadius", 0.0);
    style.line_width = shape.value("lineWidth", 0.0);
    if (shape.contains("start") && shape.contains("end")) {
        style.has_ends = true;
        style.start_x = shape["start"].value("x", 0.0);
        style.start_y = shape["start"].value("y", 0.0);
        style.end_x = shape["end"].value("x", 1.0);
        style.end_y = shape["end"].value("y", 1.0);
    }
    if (!prompt_shape(this, style)) {
        return;
    }
    session_->set_edit_name("Edit Shape");
    QString error;
    if (!session_->update_shape_layer(selected_id_.toStdString(), style, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::new_text_layer() {
    if (!session_->is_open()) {
        return;
    }
    session_->set_edit_name("New Text Layer");
    nlohmann::json style;
    style["content"] = std::string("Text");
    style["fontName"] = std::string("Helvetica");
    style["fontSize"] = 72.0;
    style["red"] = brush_color_.redF();
    style["green"] = brush_color_.greenF();
    style["blue"] = brush_color_.blueF();
    style["alignment"] = std::string("Left");
    style["tracking"] = 0.0;
    style["leading"] = 0.0;
    if (!prompt_text(this, QStringLiteral("New Text Layer"), style)) {
        return;
    }
    QString error;
    if (!session_->add_text_layer(style, 12.0, 12.0, error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::edit_text() {
    if (selected_id_.isEmpty()) {
        return;
    }
    const model::ProjectLayerRecord* record = session_->layer(selected_id_.toStdString());
    if (record == nullptr || !record->text) {
        report(QStringLiteral("Select a text layer to edit."));
        return;
    }
    nlohmann::json style = *record->text;
    if (!prompt_text(this, QStringLiteral("Edit Text"), style)) {
        return;
    }
    QString error;
    if (!session_->update_text_layer(selected_id_.toStdString(), style, error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
}

void MainWindow::text_runs() {
    if (selected_id_.isEmpty()) {
        return;
    }
    const model::ProjectLayerRecord* record = session_->layer(selected_id_.toStdString());
    if (record == nullptr || !record->text) {
        report(QStringLiteral("Select a text layer first."));
        return;
    }
    nlohmann::json style = *record->text;
    if (!prompt_text_runs(this, style)) {
        return;
    }
    session_->set_edit_name("Text Runs");
    QString error;
    if (!session_->update_text_layer(selected_id_.toStdString(), style, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::import_psd() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Import Photoshop"), QString(),
                                                      QStringLiteral("Photoshop (*.psd *.psb);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QString error;
    auto document = std::make_unique<DocumentSession>();
    if (!document->import_psd(path, error)) {
        report(error);
        return;
    }
    register_document(std::move(document));
    statusBar()->showMessage(QStringLiteral("Imported %1").arg(path), 4000);
}

void MainWindow::on_layer_item_changed(QTreeWidgetItem* item) {
    if (populating_layers_ || item == nullptr) {
        return;
    }
    const QString id = item->data(0, Qt::UserRole).toString();
    const bool visible = item->checkState(0) == Qt::Checked;
    QString error;
    if (!session_->set_visibility(id.toStdString(), visible, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
    statusBar()->showMessage(visible ? QStringLiteral("Showed layer") : QStringLiteral("Hid layer"), 3000);
}

void MainWindow::on_blend_changed(int index) {
    if (selected_id_.isEmpty() || populating_layers_) {
        return;
    }
    const std::vector<model::LayerBlendMode>& modes = model::all_blend_modes();
    if (index < 0 || index >= static_cast<int>(modes.size())) {
        return;
    }
    const model::ProjectLayerRecord* record = session_->layer(selected_id_.toStdString());
    if (record != nullptr && !record->is_group_layer() && record->effective_blend_mode() != modes[index]) {
        QString error;
        if (!session_->set_blend_mode(selected_id_.toStdString(), modes[index], error)) {
            report(error);
            return;
        }
        canvas_->updateImage(session_->image());
    }
}

}  // namespace compositor::appwin
