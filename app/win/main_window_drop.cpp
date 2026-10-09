#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>
#include <memory>
#include <string>

#include "document_session.hpp"
#include "main_window.hpp"

// Dropping files onto the window: a `.comp` opens as a project, an image opens as a new project
// (macOS's ImageFileDrop).

namespace compositor::appwin {

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasUrls()) {
        return;
    }
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString path = url.toLocalFile();
        if (path.endsWith(QStringLiteral(".comp"), Qt::CaseInsensitive)) {
            open_project(path);
            continue;
        }
        auto document = std::make_unique<DocumentSession>();
        QString error;
        if (document->import_image(path, std::string(), error)) {
            register_document(std::move(document));
        } else {
            report(error);
        }
    }
    event->acceptProposedAction();
}

}  // namespace compositor::appwin
