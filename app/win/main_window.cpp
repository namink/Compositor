#include "main_window.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "adjustment_dialog.hpp"
#include "canvas_view.hpp"
#include "color_swatch.hpp"
#include "scrub_widgets.hpp"
#include "tool_icons.hpp"

namespace compositor::appwin {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Compositor"));
    resize(1280, 800);
    setAcceptDrops(true);

    canvas_ = new CanvasView(this);
    setCentralWidget(canvas_);
    doc_info_ = new QLabel(this);
    statusBar()->addPermanentWidget(doc_info_);

    build_layers_dock();
    build_history_dock();
    build_transform_dock();
    build_palette_dock();
    build_navigator_dock();
    // Only the Layers panel shows by default, as the macOS app does; the rest open from the View menu.
    if (history_dock_ != nullptr) {
        history_dock_->hide();
    }
    if (transform_dock_ != nullptr) {
        transform_dock_->hide();
    }
    if (navigator_dock_ != nullptr) {
        navigator_dock_->hide();
    }
    if (palette_dock_ != nullptr) {
        palette_dock_->hide();
    }
    build_menus();

    // Two small panel toggles floating at the canvas's top-right, as the macOS window shows.
    canvas_overlay_ = new QWidget(canvas_);
    auto* overlay_row = new QHBoxLayout(canvas_overlay_);
    overlay_row->setContentsMargins(0, 0, 0, 0);
    overlay_row->setSpacing(4);
    const auto add_overlay_toggle = [&](const QString& icon, const QString& tip, QDockWidget* dock) {
        auto* button = new QToolButton(canvas_overlay_);
        button->setIcon(panel_icon(icon));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setToolTip(tip);
        connect(button, &QToolButton::toggled, this, [dock](bool visible) {
            if (dock != nullptr) {
                dock->setVisible(visible);
            }
        });
        overlay_row->addWidget(button);
    };
    add_overlay_toggle(QStringLiteral("history"), QStringLiteral("History"), history_dock_);
    add_overlay_toggle(QStringLiteral("navigator"), QStringLiteral("Navigator"), navigator_dock_);
    canvas_overlay_->adjustSize();
    canvas_->installEventFilter(this);
    canvas_overlay_->move(canvas_->width() - canvas_overlay_->width() - 10, 8);
    canvas_overlay_->raise();

    build_tabs();
    build_tool_toolbar();
    build_options_toolbar();
    rebuild_recent_menu();
    load_tool_defaults();
    refresh_overlays();
    load_shortcuts();

    wire_canvas();

    statusBar()->showMessage(QStringLiteral("Open a .comp project, or import an image (File menu)."));
}

void MainWindow::build_tool_toolbar() {
    auto* tools = new QToolBar(QStringLiteral("Tools"), this);
    tools->setObjectName(QStringLiteral("toolStrip"));
    tools->setOrientation(Qt::Vertical);
    tools->setMovable(false);
    tools->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tools->setIconSize(QSize(20, 20));
    addToolBar(Qt::LeftToolBarArea, tools);

    auto* group = new QActionGroup(this);
    const auto add_tool = [&](const QString& name, const QKeySequence& shortcut) {
        QAction* action = tools->addAction(tool_icon(name), name);
        action->setCheckable(true);
        action->setToolTip(shortcut.isEmpty() ? name : QStringLiteral("%1 (%2)").arg(name, shortcut.toString()));
        action->setShortcut(shortcut);
        group->addAction(action);
        register_action(QStringLiteral("tool.") + name, action);
        return action;
    };
    QAction* move = add_tool(QStringLiteral("Move"), QKeySequence(Qt::Key_V));
    QAction* rect = add_tool(QStringLiteral("Rect"), QKeySequence(Qt::Key_M));
    QAction* ellipse = add_tool(QStringLiteral("Ellipse"), QKeySequence());
    QAction* lasso = add_tool(QStringLiteral("Lasso"), QKeySequence(Qt::Key_L));
    QAction* polygon = add_tool(QStringLiteral("Poly Lasso"), QKeySequence());
    QAction* wand = add_tool(QStringLiteral("Wand"), QKeySequence(Qt::Key_W));
    QAction* eyedropper = add_tool(QStringLiteral("Eyedropper"), QKeySequence(Qt::Key_I));
    QAction* gradient = add_tool(QStringLiteral("Gradient"), QKeySequence(Qt::Key_G));
    QAction* rect_shape = add_tool(QStringLiteral("Rectangle"), QKeySequence());
    QAction* ellipse_shape = add_tool(QStringLiteral("Ellipse"), QKeySequence());
    QAction* brush = add_tool(QStringLiteral("Brush"), QKeySequence(Qt::Key_B));
    QAction* heal = add_tool(QStringLiteral("Spot Heal"), QKeySequence(Qt::Key_J));
    QAction* clone = add_tool(QStringLiteral("Clone Stamp"), QKeySequence(Qt::Key_S));
    QAction* crop = add_tool(QStringLiteral("Crop"), QKeySequence(Qt::Key_C));
    QAction* blur = add_tool(QStringLiteral("Blur"), QKeySequence(Qt::Key_R));
    QAction* pan = add_tool(QStringLiteral("Pan"), QKeySequence(Qt::Key_H));
    QAction* distort = add_tool(QStringLiteral("Distort"), QKeySequence());
    QAction* type = add_tool(QStringLiteral("Type"), QKeySequence(Qt::Key_T));
    pan->setChecked(true);

    const auto set_tool = [this](int tool) {
        ellipse_tool_ = tool == 2;
        blur_tool_ = tool == 17;
        shape_mode_ = tool == 10 ? 1 : tool == 11 ? 2 : 0;
        on_brush_changed();
        canvas_->set_move_mode(tool == 0);
        canvas_->set_select_mode(tool == 1 || tool == 2 || tool == 10 || tool == 11);
        canvas_->set_crop_mode(tool == 16);
        canvas_->set_lasso_mode(tool == 3);
        canvas_->set_polygon_mode(tool == 4);
        canvas_->set_wand_mode(tool == 5);
        canvas_->set_eyedropper_mode(tool == 6);
        canvas_->set_gradient_mode(tool == 9);
        canvas_->set_paint_mode(tool == 12 || tool == 17);
        canvas_->set_heal_mode(tool == 14);
        canvas_->set_clone_mode(tool == 15);
        distort_tool_ = tool == 18;
        canvas_->set_distort_mode(distort_tool_);
        if (distort_tool_) {
            refresh_distort_quad();
        }
        canvas_->set_text_mode(tool == 19);
        if (tool != 19) {
            finish_text_edit();
        }
        static const char* const kNames[] = {"Move",        "Marquee",    "Ellipse", "Lasso",   "Poly Lasso",
                                             "Wand",        "Eyedropper", "",        "",        "Gradient",
                                             "Rectangle",   "Ellipse",    "Brush",   "Pan",     "Spot Heal",
                                             "Clone Stamp", "Crop",       "Blur",    "Distort", "Type"};
        if (options_title_ != nullptr && tool >= 0 && tool < 20 && kNames[tool][0] != '\0') {
            options_title_->setText(QString::fromLatin1(kNames[tool]));
        }
        // Only the active tool's options show, as the macOS header does.
        const bool brush_tool = tool == 12 || tool == 14 || tool == 15 || tool == 17;
        if (brush_options_ != nullptr) {
            brush_options_->setVisible(brush_tool);
        }
        if (blur_options_ != nullptr) {
            blur_options_->setVisible(tool == 17);
        }
        if (shape_options_ != nullptr) {
            shape_options_->setVisible(tool == 10 || tool == 11);
        }
        if (wand_options_ != nullptr) {
            wand_options_->setVisible(tool == 5);
        }
    };
    connect(move, &QAction::triggered, this, [set_tool] { set_tool(0); });
    connect(rect, &QAction::triggered, this, [set_tool] { set_tool(1); });
    connect(ellipse, &QAction::triggered, this, [set_tool] { set_tool(2); });
    connect(lasso, &QAction::triggered, this, [set_tool] { set_tool(3); });
    connect(polygon, &QAction::triggered, this, [set_tool] { set_tool(4); });
    connect(wand, &QAction::triggered, this, [set_tool] { set_tool(5); });
    connect(eyedropper, &QAction::triggered, this, [set_tool] { set_tool(6); });
    connect(gradient, &QAction::triggered, this, [set_tool] { set_tool(9); });
    connect(rect_shape, &QAction::triggered, this, [set_tool] { set_tool(10); });
    connect(ellipse_shape, &QAction::triggered, this, [set_tool] { set_tool(11); });
    connect(brush, &QAction::triggered, this, [set_tool] { set_tool(12); });
    connect(heal, &QAction::triggered, this, [set_tool] { set_tool(14); });
    connect(clone, &QAction::triggered, this, [set_tool] { set_tool(15); });
    connect(crop, &QAction::triggered, this, [set_tool] { set_tool(16); });
    connect(blur, &QAction::triggered, this, [set_tool] { set_tool(17); });
    connect(pan, &QAction::triggered, this, [set_tool] { set_tool(13); });
    connect(distort, &QAction::triggered, this, [set_tool] { set_tool(18); });
    connect(type, &QAction::triggered, this, [set_tool] { set_tool(19); });

    // Push the foreground/background swatch to the bottom of the rail, as the macOS tool rail does.
    auto* spacer = new QWidget(tools);
    spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    tools->addWidget(spacer);
    color_swatch_ = new ColorSwatch(tools);
    color_swatch_->set_foreground(brush_color_);
    color_swatch_->set_background(background_color_);
    color_swatch_->on_foreground = [this](const QColor&) { choose_brush_color(); };
    color_swatch_->on_background = [this](const QColor&) {
        const QColor chosen = QColorDialog::getColor(background_color_, this, QStringLiteral("Background color"));
        if (chosen.isValid()) {
            background_color_ = chosen;
            color_swatch_->set_background(background_color_);
        }
    };
    color_swatch_->on_swap = [this] {
        std::swap(brush_color_, background_color_);
        color_swatch_->set_foreground(brush_color_);
        color_swatch_->set_background(background_color_);
        if (color_button_ != nullptr) {
            color_button_->setStyleSheet(QStringLiteral("background-color: %1").arg(brush_color_.name()));
        }
        on_brush_changed();
    };
    tools->addWidget(color_swatch_);
}

void MainWindow::on_layer_selected() {
    QTreeWidgetItem* item = layers_->currentItem();
    selected_id_ = item != nullptr ? item->data(0, Qt::UserRole).toString() : QString();
    opacity_edit_started_ = false;
    const model::ProjectLayerRecord* record =
        selected_id_.isEmpty() ? nullptr : session_->layer(selected_id_.toStdString());
    {
        const QSignalBlocker blocker(opacity_slider_);
        opacity_slider_->setValue(record != nullptr ? static_cast<int>(record->effective_opacity() * 100.0) : 100);
        opacity_value_->setText(QStringLiteral("%1 %").arg(opacity_slider_->value()));
    }
    {
        const QSignalBlocker blocker(blend_combo_);
        if (record != nullptr && !record->is_group_layer()) {
            const std::vector<model::LayerBlendMode>& modes = model::all_blend_modes();
            const auto found = std::find(modes.begin(), modes.end(), record->effective_blend_mode());
            blend_combo_->setCurrentIndex(found == modes.end() ? 0 : static_cast<int>(found - modes.begin()));
        } else {
            blend_combo_->setCurrentIndex(0);
        }
    }
    blend_combo_->setEnabled(record != nullptr && !record->is_group_layer());
    refresh_transform();
    refresh_distort_quad();
}

void MainWindow::on_opacity_changed(int percent) {
    opacity_value_->setText(QStringLiteral("%1 %").arg(percent));
    if (selected_id_.isEmpty()) {
        return;
    }
    if (!opacity_edit_started_) {
        session_->begin_interaction();
        opacity_edit_started_ = true;
    }
    QString error;
    if (!session_->set_opacity(selected_id_.toStdString(), percent / 100.0, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

void MainWindow::on_brush_changed() {
    BrushSettings brush;
    if (brush_size_ != nullptr) {
        brush.radius = brush_size_->value() / 2.0;
    }
    if (brush_hardness_ != nullptr) {
        brush.hardness = brush_hardness_->value() / 100.0;
    }
    if (brush_opacity_ != nullptr) {
        brush.opacity = brush_opacity_->value() / 100.0;
    }
    brush.erase = brush_erase_;
    brush.blur = blur_tool_;
    brush.red = brush_color_.redF();
    brush.green = brush_color_.greenF();
    brush.blue = brush_color_.blueF();
    session_->set_brush(brush);
    if (canvas_ != nullptr) {
        canvas_->set_brush_diameter(brush_size_ != nullptr ? brush_size_->value() : 0.0);
    }
    save_tool_defaults();
}

void MainWindow::open_project(const QString& path) {
    auto document = std::make_unique<DocumentSession>();
    QString error;
    if (!document->open(path, error)) {
        report(error);
        return;
    }
    register_document(std::move(document));
    add_recent(path);
    statusBar()->showMessage(QStringLiteral("Opened %1").arg(path));
}

void MainWindow::save_project(const QString& path) {
    QString error;
    if (!session_->save(path, error)) {
        report(error);
        return;
    }
    add_recent(path);
    watch_active_document();
    statusBar()->showMessage(QStringLiteral("Saved %1").arg(path));
}

void MainWindow::export_png(const QString& path) {
    QString error;
    if (!session_->export_png(path, error)) {
        report(error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Exported %1").arg(path));
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == canvas_ && event->type() == QEvent::Resize && canvas_overlay_ != nullptr) {
        canvas_overlay_->move(canvas_->width() - canvas_overlay_->width() - 10, 8);
    }
    return QMainWindow::eventFilter(watched, event);
}

}  // namespace compositor::appwin
