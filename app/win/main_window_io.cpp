#include <QFileDialog>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QStringList>
#include <memory>

#include "canvas_view.hpp"
#include "main_window.hpp"

// Image import, new/recent projects, the project digest and error reporting, split out so
// `main_window_edit.cpp` stays within the size limit.

namespace compositor::appwin {

void MainWindow::new_project() {
    bool ok = false;
    const int width =
        QInputDialog::getInt(this, QStringLiteral("New Project"), QStringLiteral("Width"), 1200, 1, 30000, 1, &ok);
    if (!ok) {
        return;
    }
    const int height =
        QInputDialog::getInt(this, QStringLiteral("New Project"), QStringLiteral("Height"), 800, 1, 30000, 1, &ok);
    if (!ok) {
        return;
    }
    auto document = std::make_unique<DocumentSession>();
    QString error;
    if (!document->create(width, height, error)) {
        report(error);
        return;
    }
    register_document(std::move(document));
    statusBar()->showMessage(QStringLiteral("New %1 x %2 project").arg(width).arg(height), 4000);
}

void MainWindow::add_recent(const QString& path) {
    QSettings settings;
    QStringList recent = settings.value(QStringLiteral("recent/projects")).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10) {
        recent.removeLast();
    }
    settings.setValue(QStringLiteral("recent/projects"), recent);
    rebuild_recent_menu();
}

void MainWindow::rebuild_recent_menu() {
    if (recent_menu_ == nullptr) {
        return;
    }
    recent_menu_->clear();
    const QStringList recent = QSettings().value(QStringLiteral("recent/projects")).toStringList();
    for (const QString& path : recent) {
        QAction* action = recent_menu_->addAction(path);
        connect(action, &QAction::triggered, this, [this, path] { open_recent(path); });
    }
    recent_menu_->setEnabled(!recent.isEmpty());
}

void MainWindow::open_recent(const QString& path) {
    open_project(path);
}

void MainWindow::show_digest() {
    QMessageBox::information(this, QStringLiteral("Project Digest"), session_->digest());
}

void MainWindow::import_image() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import Image"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.tif *.tiff *.raw *.cr2 *.cr3 *.nef *.arw *.dng *.orf *.rw2 "
                       "*.raf *.pef *.srw);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    QString error;
    if (session_ == &placeholder_ || !session_->is_open()) {
        auto document = std::make_unique<DocumentSession>();
        if (!document->import_image(path, std::string(), error)) {
            report(error);
            return;
        }
        register_document(std::move(document));
        statusBar()->showMessage(QStringLiteral("Imported %1").arg(path), 4000);
        return;
    }
    if (!session_->import_image(path, selected_id_.toStdString(), error)) {
        report(error);
        return;
    }
    refresh_layers();
    refresh_overlays();
    canvas_->updateImage(session_->image());
    statusBar()->showMessage(QStringLiteral("Imported %1").arg(path), 4000);
}

void MainWindow::report(const QString& error) {
    QMessageBox::warning(this, QStringLiteral("Compositor"), error);
    statusBar()->showMessage(error, 8000);
}

}  // namespace compositor::appwin
