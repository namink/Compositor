#include <QApplication>
#include <QFont>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <algorithm>
#include <string>

#include "canvas_view.hpp"
#include "main_window.hpp"

// The on-canvas text editor: clicking with the Type tool opens a floating editor over the text layer
// and typing updates it live (macOS's InlineTextEditor).

namespace compositor::appwin {
namespace {

[[nodiscard]] bool contains_point(const model::LayerTransform& transform, double x, double y) {
    return x >= transform.origin_x && y >= transform.origin_y && x < transform.origin_x + transform.width &&
           y < transform.origin_y + transform.height;
}

}  // namespace

void MainWindow::text_tool_click(double document_x, double document_y) {
    if (!session_->is_open()) {
        return;
    }
    if (text_editor_ != nullptr && text_editor_->isVisible()) {
        finish_text_edit();
        return;
    }
    const model::ProjectManifest* manifest = session_->manifest();
    if (manifest == nullptr) {
        return;
    }
    for (const model::ProjectLayerRecord& record : manifest->layers) {
        if (record.text && !record.is_group_layer() && contains_point(record.transform, document_x, document_y)) {
            begin_text_edit(record.id, false);
            return;
        }
    }
    nlohmann::json style;
    style["content"] = std::string("Text");
    style["fontName"] = std::string("Helvetica");
    style["fontSize"] = 48.0;
    style["red"] = brush_color_.redF();
    style["green"] = brush_color_.greenF();
    style["blue"] = brush_color_.blueF();
    style["alignment"] = std::string("Left");
    style["tracking"] = 0.0;
    style["leading"] = 0.0;
    session_->set_edit_name("New Text Layer");
    QString error;
    if (!session_->add_text_layer(style, document_x, document_y, error)) {
        report(error);
        return;
    }
    refresh_layers();
    canvas_->updateImage(session_->image());
    begin_text_edit(*session_->manifest()->active_layer_id, true);
}

void MainWindow::begin_text_edit(const std::string& id, bool is_new) {
    const model::ProjectLayerRecord* record = session_->layer(id);
    if (record == nullptr || !record->text) {
        return;
    }
    editing_text_id_ = id;
    editing_text_style_ = *record->text;
    editing_text_new_ = is_new;
    if (text_editor_ == nullptr) {
        text_editor_ = new QPlainTextEdit(canvas_);
        text_editor_->setStyleSheet(
            QStringLiteral("background: rgba(20,20,20,180); color: #f0f0f0; border: 1px solid #2a82da;"));
        connect(text_editor_, &QPlainTextEdit::textChanged, this, [this] { update_text_edit(); });
        connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) {
            if (text_editor_ != nullptr && text_editor_->isVisible() && now != text_editor_ &&
                (now == nullptr || now->parent() != text_editor_)) {
                finish_text_edit();
            }
        });
    }
    const double scale = canvas_->view_scale();
    const model::LayerTransform& transform = record->transform;
    const QPointF position = canvas_->document_to_widget(QPointF(transform.origin_x, transform.origin_y));
    text_editor_->setGeometry(static_cast<int>(position.x()), static_cast<int>(position.y()),
                              std::max(160, static_cast<int>(transform.width * scale)),
                              std::max(44, static_cast<int>(transform.height * scale)));
    QFont font;
    font.setPointSizeF(std::max(8.0, editing_text_style_.value("fontSize", 48.0) * scale * 0.75));
    text_editor_->setFont(font);
    const QString content = QString::fromStdString(editing_text_style_.value("content", std::string()));
    {
        const QSignalBlocker blocker(text_editor_);
        text_editor_->setPlainText(content);
    }
    text_editor_->show();
    text_editor_->setFocus();
    text_editor_->selectAll();
}

void MainWindow::update_text_edit() {
    if (text_editor_ == nullptr || editing_text_id_.empty()) {
        return;
    }
    editing_text_style_["content"] = text_editor_->toPlainText().toStdString();
    QString error;
    if (!session_->update_text_layer(editing_text_id_, editing_text_style_, error)) {
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::finish_text_edit() {
    if (text_editor_ == nullptr || editing_text_id_.empty()) {
        return;
    }
    const bool empty = text_editor_->toPlainText().trimmed().isEmpty();
    const bool was_new = editing_text_new_;
    const std::string id = editing_text_id_;
    text_editor_->hide();
    editing_text_id_.clear();
    editing_text_new_ = false;
    if (was_new && empty) {
        QString error;
        session_->delete_layer(id, error);
        refresh_layers();
    }
    canvas_->updateImage(session_->image());
    canvas_->setFocus();
}

}  // namespace compositor::appwin
