#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QVBoxLayout>
#include <cstdint>
#include <memory>

#include "canvas_view.hpp"
#include "main_window.hpp"

// Image import, new/recent projects, the project digest and error reporting, split out so
// `main_window_edit.cpp` stays within the size limit.

namespace compositor::appwin {

void MainWindow::new_project() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("New Canvas"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    layout->addLayout(form);
    auto* width = new QSpinBox(&dialog);
    width->setRange(1, 30000);
    width->setValue(1200);
    form->addRow(QStringLiteral("Width (px)"), width);
    auto* height = new QSpinBox(&dialog);
    height->setRange(1, 30000);
    height->setValue(800);
    form->addRow(QStringLiteral("Height (px)"), height);
    auto* background = new QComboBox(&dialog);
    background->addItems({QStringLiteral("Transparent"), QStringLiteral("White"), QStringLiteral("Black")});
    form->addRow(QStringLiteral("Background"), background);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const std::uint32_t fill = background->currentIndex() == 1   ? 0xFFFFFFFFU
                               : background->currentIndex() == 2 ? 0xFF000000U
                                                                 : 0U;
    auto document = std::make_unique<DocumentSession>();
    QString error;
    if (!document->create(width->value(), height->value(), fill, error)) {
        report(error);
        return;
    }
    register_document(std::move(document));
    statusBar()->showMessage(QStringLiteral("New %1 x %2 project").arg(width->value()).arg(height->value()), 4000);
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
