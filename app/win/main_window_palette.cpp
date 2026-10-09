#include <QColor>
#include <QDockWidget>
#include <QIcon>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>
#include <QWidget>

#include "main_window.hpp"

// A color palette (macOS's ColorPalette): swatches that set the brush color, and a button to add the
// current color to the palette.

namespace compositor::appwin {
namespace {

const QColor kSwatches[] = {QColor(0, 0, 0),       QColor(64, 64, 64),   QColor(128, 128, 128), QColor(192, 192, 192),
                            QColor(255, 255, 255), QColor(220, 40, 40),  QColor(240, 150, 40),  QColor(240, 220, 60),
                            QColor(60, 190, 90),   QColor(40, 170, 220), QColor(60, 80, 220),   QColor(150, 70, 200)};

}  // namespace

void MainWindow::build_palette_dock() {
    auto* dock = new QDockWidget(QStringLiteral("Color"), this);
    auto* panel = new QWidget(dock);
    auto* layout = new QVBoxLayout(panel);
    palette_ = new QListWidget(panel);
    palette_->setViewMode(QListWidget::IconMode);
    palette_->setIconSize(QSize(24, 24));
    palette_->setGridSize(QSize(30, 30));
    palette_->setResizeMode(QListWidget::Adjust);
    palette_->setMovement(QListWidget::Static);
    for (const QColor& color : kSwatches) {
        QPixmap swatch(24, 24);
        swatch.fill(color);
        auto* item = new QListWidgetItem(QIcon(swatch), QString());
        item->setData(Qt::UserRole, color.name());
        palette_->addItem(item);
    }
    connect(palette_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item == nullptr) {
            return;
        }
        const QColor chosen(item->data(Qt::UserRole).toString());
        if (!chosen.isValid()) {
            return;
        }
        brush_color_ = chosen;
        if (color_button_ != nullptr) {
            color_button_->setStyleSheet(QStringLiteral("background-color: %1").arg(brush_color_.name()));
        }
        on_brush_changed();
    });
    layout->addWidget(palette_, 1);
    auto* add = new QPushButton(QStringLiteral("Add Current Color"), panel);
    connect(add, &QPushButton::clicked, this, [this] {
        QPixmap swatch(24, 24);
        swatch.fill(brush_color_);
        auto* item = new QListWidgetItem(QIcon(swatch), QString());
        item->setData(Qt::UserRole, brush_color_.name());
        palette_->addItem(item);
    });
    layout->addWidget(add);

    dock->setWidget(panel);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    palette_dock_ = dock;
}

}  // namespace compositor::appwin
