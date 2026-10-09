#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMessageBox>
#include <QString>

#include "canvas_view.hpp"
#include "main_window.hpp"

// Watching the open project's file for external changes and offering a reload, ported from the macOS
// app's `ProjectWatcher`: the app never applies an external rewrite on its own, it asks first.

namespace compositor::appwin {

void MainWindow::watch_active_document() {
    if (watcher_ == nullptr) {
        watcher_ = new QFileSystemWatcher(this);
        connect(watcher_, &QFileSystemWatcher::fileChanged, this,
                [this](const QString& path) { on_file_changed(path); });
    }
    if (!watched_path_.isEmpty() && watcher_->files().contains(watched_path_)) {
        watcher_->removePath(watched_path_);
    }
    watched_path_.clear();
    watched_mtime_ = 0;
    if (session_ == nullptr || !session_->is_open() || session_->path().empty()) {
        return;
    }
    const QString path = QString::fromStdString(session_->path());
    if (!QFileInfo::exists(path)) {
        return;
    }
    watcher_->addPath(path);
    watched_path_ = path;
    watched_mtime_ = QFileInfo(path).lastModified().toMSecsSinceEpoch();
}

void MainWindow::on_file_changed(const QString& path) {
    if (path != watched_path_) {
        return;
    }
    const QFileInfo info(path);
    if (!info.exists()) {
        return;
    }
    const qint64 modified = info.lastModified().toMSecsSinceEpoch();
    if (modified == watched_mtime_) {
        return;  // our own save; nothing external happened
    }
    watched_mtime_ = modified;
    // Some editors replace the file, dropping the watch; keep it if it survived.
    if (!watcher_->files().contains(path)) {
        watcher_->addPath(path);
    }
    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, QStringLiteral("File changed"),
                              QStringLiteral("The project changed on disk.\nReload it? Unsaved edits will be lost."),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!session_->reload(error)) {
        report(error);
        return;
    }
    refresh_layers();
    refresh_overlays();
    canvas_->updateImage(session_->image());
}

}  // namespace compositor::appwin
