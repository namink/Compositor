#include <QDockWidget>
#include <QFont>
#include <QListWidget>
#include <QListWidgetItem>
#include <string>
#include <vector>

#include "canvas_view.hpp"
#include "main_window.hpp"

// The History panel: each named edit is a row, and clicking one restores that state (macOS's
// DocumentHistory). Kept in its own file to stay within the size limit.

namespace compositor::appwin {

void MainWindow::build_history_dock() {
    auto* dock = new QDockWidget(QStringLiteral("History"), this);
    history_ = new QListWidget(dock);
    history_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(history_, &QListWidget::itemClicked, this, [this](QListWidgetItem*) { on_history_clicked(); });
    dock->setWidget(history_);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    history_dock_ = dock;
}

void MainWindow::refresh_history() {
    if (history_ == nullptr) {
        return;
    }
    populating_history_ = true;
    history_->clear();
    if (session_ != nullptr && session_->is_open()) {
        const std::vector<std::string> labels = session_->history_labels();
        for (std::size_t i = 0; i < labels.size(); ++i) {
            auto* item = new QListWidgetItem(QString::fromStdString(labels[i]), history_);
            if (static_cast<int>(i) == session_->history_current()) {
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
            }
        }
        history_->setCurrentRow(session_->history_current());
    }
    populating_history_ = false;
}

void MainWindow::on_history_clicked() {
    if (populating_history_ || history_ == nullptr || session_ == nullptr) {
        return;
    }
    const int row = history_->currentRow();
    if (row < 0) {
        return;
    }
    QString error;
    if (!session_->jump_history(row, error)) {
        report(error);
        return;
    }
    refresh_layers();
    refresh_selection_outline();
    canvas_->updateImage(session_->image());
}

}  // namespace compositor::appwin
