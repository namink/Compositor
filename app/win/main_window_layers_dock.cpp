#include <QComboBox>
#include <QDockWidget>
#include <QFont>
#include <QLabel>
#include <QMenu>
#include <QSlider>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>
#include <string>

#include "adjustment_dialog.hpp"
#include "main_window.hpp"
#include "tool_icons.hpp"

// The Layers dock, split out so main_window.cpp stays within the soft size limit.

namespace compositor::appwin {
void MainWindow::build_layers_dock() {
    auto* dock = new QDockWidget(QStringLiteral("Layers"), this);
    auto* panel = new QWidget(dock);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 10, 8);
    layout->setSpacing(6);

    // Header: "Layers" with the layer count, as the macOS panel does (12 pt semibold, count tertiary).
    auto* header = new QHBoxLayout();
    auto* title = new QLabel(QStringLiteral("Layers"), panel);
    QFont title_font = title->font();
    title_font.setPointSizeF(10.0);
    title_font.setBold(true);
    title->setFont(title_font);
    layer_count_ = new QLabel(QStringLiteral("0"), panel);
    layer_count_->setStyleSheet(QStringLiteral("color: #8a8a8a;"));
    header->addWidget(title);
    header->addStretch(1);
    header->addWidget(layer_count_);
    layout->addLayout(header);

    auto* grid = new QGridLayout();
    grid->addWidget(new QLabel(QStringLiteral("Blend"), panel), 0, 0);
    blend_combo_ = new QComboBox(panel);
    for (const model::LayerBlendMode mode : model::all_blend_modes()) {
        blend_combo_->addItem(QString::fromStdString(std::string(model::to_string(mode))));
    }
    connect(blend_combo_, &QComboBox::currentIndexChanged, this, [this](int index) { on_blend_changed(index); });
    grid->addWidget(blend_combo_, 0, 1);

    grid->addWidget(new QLabel(QStringLiteral("Opacity"), panel), 1, 0);
    auto* opacity_row = new QWidget(panel);
    auto* opacity_layout = new QGridLayout(opacity_row);
    opacity_layout->setContentsMargins(0, 0, 0, 0);
    opacity_slider_ = new QSlider(Qt::Horizontal, opacity_row);
    opacity_slider_->setRange(0, 100);
    opacity_slider_->setValue(100);
    opacity_value_ = new QLabel(QStringLiteral("100 %"), opacity_row);
    opacity_value_->setMinimumWidth(44);
    connect(opacity_slider_, &QSlider::valueChanged, this, [this](int value) { on_opacity_changed(value); });
    opacity_layout->addWidget(opacity_slider_, 0, 0);
    opacity_layout->addWidget(opacity_value_, 0, 1);
    grid->addWidget(opacity_row, 1, 1);
    layout->addLayout(grid);

    layers_ = new QTreeWidget(panel);
    layers_->setHeaderHidden(true);
    layers_->setIndentation(14);
    layers_->setIconSize(QSize(36, 36));
    layers_->setSelectionMode(QAbstractItemView::SingleSelection);
    layers_->setDragDropMode(QAbstractItemView::InternalMove);
    layers_->setDefaultDropAction(Qt::MoveAction);
    // Drag to restack or to drop into a folder: the macOS app's Layer > Arrange plus panel nesting.
    connect(layers_, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem* item, int) { on_layer_item_changed(item); });
    connect(layers_->model(), &QAbstractItemModel::rowsMoved, this, [this] { on_layers_reordered(); });
    layout->addWidget(layers_, 1);

    // Footer: new layer / folder / mask / effects / adjustment / delete, as the macOS panel's row.
    auto* footer = new QHBoxLayout();
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(2);
    const auto add_footer = [&](const QString& icon, const QString& tip, const std::function<void()>& action) {
        auto* button = new QToolButton(panel);
        button->setIcon(panel_icon(icon));
        button->setIconSize(QSize(18, 18));
        button->setToolTip(tip);
        button->setAutoRaise(true);
        QFont font = button->font();
        font.setPointSizeF(11.0);
        button->setFont(font);
        connect(button, &QToolButton::clicked, this, action);
        footer->addWidget(button);
        return button;
    };
    add_footer(QStringLiteral("plus"), QStringLiteral("New Layer"), [this] { add_layer(); });
    add_footer(QStringLiteral("folder"), QStringLiteral("New Folder"), [this] { new_folder(); });
    auto* mask_button =
        add_footer(QStringLiteral("mask"), QStringLiteral("Layer Mask"), [this] { add_mask(true, false); });
    auto* mask_menu = new QMenu(mask_button);
    mask_menu->addAction(QStringLiteral("Reveal All"), this, [this] { add_mask(false, false); });
    mask_menu->addAction(QStringLiteral("Hide All"), this, [this] { add_mask(false, true); });
    mask_menu->addAction(QStringLiteral("From Selection"), this, [this] { add_mask(true, false); });
    mask_menu->addAction(QStringLiteral("Delete Mask"), this, [this] { remove_mask(); });
    mask_button->setMenu(mask_menu);
    mask_button->setPopupMode(QToolButton::DelayedPopup);
    add_footer(QStringLiteral("effects"), QStringLiteral("Layer Effects"), [this] { layer_effects(); });
    auto* adjust_button = add_footer(QStringLiteral("adjustment"), QStringLiteral("New Adjustment Layer"), [] {});
    auto* adjust_menu = new QMenu(adjust_button);
    for (const QString& kind : adjustment_kinds()) {
        adjust_menu->addAction(kind, this, [this, kind] { new_adjustment_layer(kind); });
    }
    adjust_button->setMenu(adjust_menu);
    adjust_button->setPopupMode(QToolButton::InstantPopup);
    footer->addStretch(1);
    add_footer(QStringLiteral("trash"), QStringLiteral("Delete Layer"), [this] { delete_layer(); });
    layout->addLayout(footer);

    dock->setWidget(panel);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setMinimumWidth(252);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}
}  // namespace compositor::appwin
