#include <QLabel>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <algorithm>

#include "canvas_view.hpp"
#include "main_window.hpp"

// Multiple open documents, one tab each. The window keeps one canvas and one Layers panel; a tab
// switch points `session_` at the active document and refreshes them. Split out so
// `main_window.cpp` stays within the size limit.

namespace compositor::appwin {

void MainWindow::build_tabs() {
    auto* bar = new QToolBar(QStringLiteral("Documents"), this);
    bar->setObjectName(QStringLiteral("documentTabs"));
    bar->setMovable(false);
    tabs_ = new QTabBar(bar);
    tabs_->setExpanding(false);
    tabs_->setTabsClosable(true);
    tabs_->setMovable(true);
    tabs_->setUsesScrollButtons(true);  // overflow scrolls, like the macOS strip's hidden tabs
    tabs_->setElideMode(Qt::ElideRight);
    connect(tabs_, &QTabBar::currentChanged, this, [this](int index) { switch_document(index); });
    connect(tabs_, &QTabBar::tabCloseRequested, this, [this](int index) { close_document(index); });
    bar->addWidget(tabs_);
    auto* add_tab = new QToolButton(bar);
    add_tab->setText(QStringLiteral("+"));
    add_tab->setToolTip(QStringLiteral("New project"));
    add_tab->setAutoRaise(true);
    connect(add_tab, &QToolButton::clicked, this, [this] { new_project(); });
    bar->addWidget(add_tab);
    addToolBar(Qt::TopToolBarArea, bar);
}

void MainWindow::register_document(std::unique_ptr<DocumentSession> document) {
    documents_.push_back(std::move(document));
    const int index = static_cast<int>(documents_.size()) - 1;
    tabs_->addTab(QStringLiteral("Document %1").arg(index + 1));
    tabs_->setCurrentIndex(index);
    session_ = documents_[static_cast<std::size_t>(index)].get();
    show_active_document();
}

void MainWindow::switch_document(int index) {
    if (index < 0 || index >= static_cast<int>(documents_.size())) {
        return;
    }
    current_document_ = index;
    session_ = documents_[static_cast<std::size_t>(index)].get();
    show_active_document();
}

void MainWindow::close_document(int index) {
    if (index < 0 || index >= static_cast<int>(documents_.size())) {
        return;
    }
    const bool was_current = index == current_document_;
    documents_.erase(documents_.begin() + index);
    tabs_->removeTab(index);
    if (documents_.empty()) {
        current_document_ = -1;
        session_ = &placeholder_;
        show_active_document();
        return;
    }
    if (was_current) {
        const int next = std::min(index, static_cast<int>(documents_.size()) - 1);
        current_document_ = next;
        session_ = documents_[static_cast<std::size_t>(next)].get();
        tabs_->setCurrentIndex(next);
        show_active_document();
    }
}

void MainWindow::show_active_document() {
    if (session_ == nullptr || !session_->is_open()) {
        canvas_->clear();
        layers_->clear();
        canvas_->set_selection_outline({});
        refresh_overlays();
        if (doc_info_ != nullptr) {
            doc_info_->clear();
        }
        watch_active_document();
        return;
    }
    canvas_->set_selection_outline({});
    canvas_->setImage(session_->image());
    refresh_layers();
    refresh_overlays();
    watch_active_document();
    const model::ProjectManifest* manifest = session_->manifest();
    if (doc_info_ != nullptr && manifest != nullptr) {
        doc_info_->setText(QStringLiteral("%1 x %2 px  ·  %3  ·  Transparent")
                               .arg(manifest->width)
                               .arg(manifest->height)
                               .arg(QString::fromStdString(manifest->color_space)));
    }
}

}  // namespace compositor::appwin
