#include <QAction>
#include <QDockWidget>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QString>
#include <utility>
#include <vector>

#include "adjustment_dialog.hpp"
#include "canvas_view.hpp"
#include "main_window.hpp"

// The menu bar, split out so `main_window.cpp` stays within the size limit.

namespace compositor::appwin {
namespace {

constexpr const char* kProjectFilter = "Compositor project (*.comp);;All files (*)";
constexpr const char* kPngFilter = "PNG image (*.png);;All files (*)";

}  // namespace

void MainWindow::build_menus() {
    QMenu* file = menuBar()->addMenu(QStringLiteral("&File"));
    QAction* new_project_action = file->addAction(QStringLiteral("&New Project..."));
    new_project_action->setShortcut(QKeySequence::New);
    connect(new_project_action, &QAction::triggered, this, [this] { new_project(); });
    QAction* open = file->addAction(QStringLiteral("&Open Project..."));
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Compositor project"), QString(),
                                                          QString::fromUtf8(kProjectFilter));
        if (!path.isEmpty()) {
            open_project(path);
        }
    });
    recent_menu_ = file->addMenu(QStringLiteral("Open &Recent"));
    QAction* save = file->addAction(QStringLiteral("&Save Project As..."));
    save->setShortcut(QKeySequence::SaveAs);
    connect(save, &QAction::triggered, this, [this] {
        const QString path =
            QFileDialog::getSaveFileName(this, QStringLiteral("Save Compositor project"),
                                         QStringLiteral("Untitled.comp"), QString::fromUtf8(kProjectFilter));
        if (!path.isEmpty()) {
            save_project(path);
        }
    });
    QAction* export_action = file->addAction(QStringLiteral("&Export PNG..."));
    connect(export_action, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export PNG"),
                                                          QStringLiteral("export.png"), QString::fromUtf8(kPngFilter));
        if (!path.isEmpty()) {
            export_png(path);
        }
    });
    QAction* export_jpeg_action = file->addAction(QStringLiteral("Export &JPEG..."));
    connect(export_jpeg_action, &QAction::triggered, this, [this] { export_jpeg_menu(); });
    QAction* export_psd_action = file->addAction(QStringLiteral("Export &Photoshop (PSD)..."));
    connect(export_psd_action, &QAction::triggered, this, [this] { export_psd_menu(); });
    file->addSeparator();
    QAction* import_action = file->addAction(QStringLiteral("&Import Image..."));
    connect(import_action, &QAction::triggered, this, [this] { import_image(); });
    QAction* import_psd_action = file->addAction(QStringLiteral("Import &Photoshop (PSD)..."));
    connect(import_psd_action, &QAction::triggered, this, [this] { import_psd(); });
    file->addSeparator();
    QAction* quit = file->addAction(QStringLiteral("E&xit"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    QMenu* edit = menuBar()->addMenu(QStringLiteral("&Edit"));
    QAction* undo_action = edit->addAction(QStringLiteral("&Undo"));
    undo_action->setShortcut(QKeySequence::Undo);
    connect(undo_action, &QAction::triggered, this, [this] { undo(); });
    QAction* redo_action = edit->addAction(QStringLiteral("&Redo"));
    redo_action->setShortcut(QKeySequence::Redo);
    connect(redo_action, &QAction::triggered, this, [this] { redo(); });
    edit->addSeparator();
    QAction* delete_action = edit->addAction(QStringLiteral("&Delete"));
    delete_action->setShortcut(QKeySequence::Delete);
    connect(delete_action, &QAction::triggered, this, [this] { delete_selection(); });
    QAction* fill_action = edit->addAction(QStringLiteral("&Fill Selection"));
    fill_action->setShortcut(QKeySequence(QStringLiteral("Shift+F5")));
    connect(fill_action, &QAction::triggered, this, [this] { fill_selection(); });
    QAction* content_fill_action = edit->addAction(QStringLiteral("Content-&Aware Fill"));
    connect(content_fill_action, &QAction::triggered, this, [this] { content_aware_fill(); });
    QAction* float_action = edit->addAction(QStringLiteral("&Float Selection"));
    connect(float_action, &QAction::triggered, this, [this] { float_selection(); });
    edit->addSeparator();
    QAction* cut_action = edit->addAction(QStringLiteral("Cu&t"));
    cut_action->setShortcut(QKeySequence::Cut);
    connect(cut_action, &QAction::triggered, this, [this] { cut_selection(); });
    QAction* copy_action = edit->addAction(QStringLiteral("&Copy"));
    copy_action->setShortcut(QKeySequence::Copy);
    connect(copy_action, &QAction::triggered, this, [this] { copy_selection(); });
    QAction* paste_action = edit->addAction(QStringLiteral("&Paste"));
    paste_action->setShortcut(QKeySequence::Paste);
    connect(paste_action, &QAction::triggered, this, [this] { paste(); });
    edit->addSeparator();
    QAction* copy_merged_action = edit->addAction(QStringLiteral("Copy &Merged"));
    copy_merged_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
    connect(copy_merged_action, &QAction::triggered, this, [this] { copy_merged(); });
    edit->addSeparator();
    QAction* properties_action = edit->addAction(QStringLiteral("Adjustment &Properties..."));
    connect(properties_action, &QAction::triggered, this, [this] { edit_adjustment(); });
    QAction* shortcuts_action = edit->addAction(QStringLiteral("&Keyboard Shortcuts..."));
    connect(shortcuts_action, &QAction::triggered, this, [this] { show_shortcuts_dialog(); });

    QMenu* view = menuBar()->addMenu(QStringLiteral("&View"));
    QAction* zoom_in = view->addAction(QStringLiteral("Zoom &In"));
    zoom_in->setShortcut(QKeySequence::ZoomIn);
    connect(zoom_in, &QAction::triggered, canvas_, &CanvasView::zoom_in);
    QAction* zoom_out = view->addAction(QStringLiteral("Zoom &Out"));
    zoom_out->setShortcut(QKeySequence::ZoomOut);
    connect(zoom_out, &QAction::triggered, canvas_, &CanvasView::zoom_out);
    QAction* fit = view->addAction(QStringLiteral("&Fit on Screen"));
    fit->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(fit, &QAction::triggered, canvas_, &CanvasView::zoom_fit);
    QAction* actual = view->addAction(QStringLiteral("&Actual Pixels"));
    actual->setShortcut(QKeySequence(QStringLiteral("Ctrl+1")));
    connect(actual, &QAction::triggered, canvas_, &CanvasView::zoom_actual);
    view->addSeparator();
    QAction* grid_action = view->addAction(QStringLiteral("Show &Grid"));
    grid_action->setCheckable(true);
    connect(grid_action, &QAction::toggled, this, [this](bool on) { toggle_grid(on); });
    QAction* grid_settings_action = view->addAction(QStringLiteral("Grid &Settings..."));
    connect(grid_settings_action, &QAction::triggered, this, [this] { grid_settings(); });
    QAction* rulers_action = view->addAction(QStringLiteral("Show &Rulers"));
    rulers_action->setCheckable(true);
    rulers_action->setChecked(shows_rulers_);
    connect(rulers_action, &QAction::toggled, this, [this](bool checked) { toggle_rulers(checked); });
    view->addSeparator();
    const auto add_panel_toggle = [this, view](const QString& label, QDockWidget* dock) {
        if (dock == nullptr) {
            return;
        }
        QAction* action = view->addAction(label);
        action->setCheckable(true);
        action->setChecked(dock->isVisible());
        connect(action, &QAction::toggled, this, [dock](bool visible) { dock->setVisible(visible); });
    };
    add_panel_toggle(QStringLiteral("&Color Panel"), palette_dock_);
    add_panel_toggle(QStringLiteral("&History"), history_dock_);
    add_panel_toggle(QStringLiteral("&Transform"), transform_dock_);
    add_panel_toggle(QStringLiteral("&Navigator"), navigator_dock_);
    view->addSeparator();
    QAction* guide_action = view->addAction(QStringLiteral("New &Guide..."));
    connect(guide_action, &QAction::triggered, this, [this] { new_guide(); });
    QAction* clear_guides_action = view->addAction(QStringLiteral("&Clear Guides"));
    connect(clear_guides_action, &QAction::triggered, this, [this] { clear_guides(); });

    QMenu* select_menu = menuBar()->addMenu(QStringLiteral("&Select"));
    QAction* select_all_action = select_menu->addAction(QStringLiteral("Select &All"));
    select_all_action->setShortcut(QKeySequence::SelectAll);
    connect(select_all_action, &QAction::triggered, this, [this] { select_all(); });
    QAction* deselect_action = select_menu->addAction(QStringLiteral("&Deselect"));
    deselect_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    connect(deselect_action, &QAction::triggered, this, [this] { deselect(); });
    select_menu->addSeparator();
    QAction* inverse_action = select_menu->addAction(QStringLiteral("&Inverse"));
    inverse_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+I")));
    connect(inverse_action, &QAction::triggered, this, [this] { invert_selection(); });
    QAction* grow_action = select_menu->addAction(QStringLiteral("&Grow"));
    connect(grow_action, &QAction::triggered, this, [this] { grow_selection(); });
    QAction* shrink_action = select_menu->addAction(QStringLiteral("&Shrink"));
    connect(shrink_action, &QAction::triggered, this, [this] { shrink_selection(); });
    QAction* feather_action = select_menu->addAction(QStringLiteral("&Feather"));
    connect(feather_action, &QAction::triggered, this, [this] { feather_selection(); });
    select_menu->addSeparator();
    QAction* load_layer_action = select_menu->addAction(QStringLiteral("Load Layer &Selection"));
    connect(load_layer_action, &QAction::triggered, this, [this] { load_layer_selection(); });
    QAction* load_mask_action = select_menu->addAction(QStringLiteral("Load &Mask Selection"));
    connect(load_mask_action, &QAction::triggered, this, [this] { load_mask_selection(); });
    QAction* color_range_action = select_menu->addAction(QStringLiteral("&Color Range..."));
    connect(color_range_action, &QAction::triggered, this, [this] { color_range(); });
    QAction* refine_action = select_menu->addAction(QStringLiteral("Re&fine Edge..."));
    connect(refine_action, &QAction::triggered, this, [this] { refine_edge(); });

    QMenu* image = menuBar()->addMenu(QStringLiteral("&Image"));
    QAction* crop_action = image->addAction(QStringLiteral("&Crop to Selection"));
    connect(crop_action, &QAction::triggered, this, [this] { crop_to_selection(); });
    QAction* flatten_action = image->addAction(QStringLiteral("&Flatten Image"));
    connect(flatten_action, &QAction::triggered, this, [this] { flatten(); });
    image->addSeparator();
    QAction* flip_h = image->addAction(QStringLiteral("Flip &Horizontal"));
    connect(flip_h, &QAction::triggered, this, [this] { flip_horizontal(); });
    QAction* flip_v = image->addAction(QStringLiteral("Flip &Vertical"));
    connect(flip_v, &QAction::triggered, this, [this] { flip_vertical(); });
    QAction* rot_cw = image->addAction(QStringLiteral("Rotate 90\u00B0 &Clockwise"));
    connect(rot_cw, &QAction::triggered, this, [this] { rotate_cw(); });
    QAction* rot_ccw = image->addAction(QStringLiteral("Rotate 90\u00B0 C&ounterclockwise"));
    connect(rot_ccw, &QAction::triggered, this, [this] { rotate_ccw(); });
    QAction* reset = image->addAction(QStringLiteral("&Reset Transform"));
    connect(reset, &QAction::triggered, this, [this] { reset_transform(); });
    image->addSeparator();
    QAction* flip_ch = image->addAction(QStringLiteral("Flip Canvas &Horizontal"));
    connect(flip_ch, &QAction::triggered, this, [this] { flip_canvas_horizontal(); });
    QAction* flip_cv = image->addAction(QStringLiteral("Flip Canvas &Vertical"));
    connect(flip_cv, &QAction::triggered, this, [this] { flip_canvas_vertical(); });
    QAction* rotate_canvas_cw_action = image->addAction(QStringLiteral("Rotate Canvas 90\u00B0 Clock&wise"));
    connect(rotate_canvas_cw_action, &QAction::triggered, this, [this] { rotate_canvas_cw(); });
    QAction* rotate_canvas_ccw_action = image->addAction(QStringLiteral("Rotate Canvas 90\u00B0 C&ounterclockwise"));
    connect(rotate_canvas_ccw_action, &QAction::triggered, this, [this] { rotate_canvas_ccw(); });
    image->addSeparator();
    QAction* canvas_size_action = image->addAction(QStringLiteral("Canvas Si&ze..."));
    connect(canvas_size_action, &QAction::triggered, this, [this] { canvas_size_menu(); });
    QAction* image_size_action = image->addAction(QStringLiteral("&Image Size..."));
    connect(image_size_action, &QAction::triggered, this, [this] { image_size_menu(); });
    QAction* trim_action = image->addAction(QStringLiteral("&Trim"));
    connect(trim_action, &QAction::triggered, this, [this] { trim_canvas(); });
    QMenu* auto_levels_menu = image->addMenu(QStringLiteral("&Auto Levels"));
    const std::vector<std::pair<QString, int>> auto_modes{
        {QStringLiteral("Contrast"), 0}, {QStringLiteral("Color"), 1}, {QStringLiteral("Color + neutral midtones"), 2}};
    for (const auto& [label, mode] : auto_modes) {
        QAction* action = auto_levels_menu->addAction(label);
        connect(action, &QAction::triggered, this, [this, mode] { auto_levels(mode); });
    }

    QMenu* filter = menuBar()->addMenu(QStringLiteral("Fi&lter"));
    QAction* last_filter_action = filter->addAction(QStringLiteral("&Last Filter"));
    connect(last_filter_action, &QAction::triggered, this, [this] { last_filter(); });
    filter->addSeparator();
    QAction* camera_raw_color_action = filter->addAction(QStringLiteral("Camera Raw &Color..."));
    connect(camera_raw_color_action, &QAction::triggered, this, [this] { camera_raw_color(); });
    filter->addSeparator();
    for (const QString& kind : filter_kinds()) {
        QAction* action = filter->addAction(kind);
        connect(action, &QAction::triggered, this, [this, kind] { filter_layer(kind); });
    }

    QMenu* layer = menuBar()->addMenu(QStringLiteral("&Layer"));
    QMenu* adjustments = layer->addMenu(QStringLiteral("New &Adjustment Layer"));
    for (const QString& kind : adjustment_kinds()) {
        QAction* action = adjustments->addAction(kind);
        connect(action, &QAction::triggered, this, [this, kind] { new_adjustment_layer(kind); });
    }
    layer->addSeparator();
    QAction* new_layer = layer->addAction(QStringLiteral("&New Layer"));
    connect(new_layer, &QAction::triggered, this, [this] { add_layer(); });
    QAction* duplicate = layer->addAction(QStringLiteral("&Duplicate Layer"));
    connect(duplicate, &QAction::triggered, this, [this] { duplicate_layer(); });
    QAction* via_copy = layer->addAction(QStringLiteral("Layer via Cop&y"));
    via_copy->setShortcut(QKeySequence(QStringLiteral("Ctrl+J")));
    connect(via_copy, &QAction::triggered, this, [this] { layer_via_copy(); });
    QAction* edit_shape_action = layer->addAction(QStringLiteral("Edit S&hape..."));
    connect(edit_shape_action, &QAction::triggered, this, [this] { edit_shape(); });
    QAction* effects_action = layer->addAction(QStringLiteral("Layer &Effects..."));
    connect(effects_action, &QAction::triggered, this, [this] { layer_effects(); });
    QAction* text_layer = layer->addAction(QStringLiteral("New &Text Layer..."));
    connect(text_layer, &QAction::triggered, this, [this] { new_text_layer(); });
    QAction* edit_text_action = layer->addAction(QStringLiteral("Edit Te&xt..."));
    connect(edit_text_action, &QAction::triggered, this, [this] { edit_text(); });
    QAction* text_runs_action = layer->addAction(QStringLiteral("Text &Runs..."));
    connect(text_runs_action, &QAction::triggered, this, [this] { text_runs(); });
    QAction* remove = layer->addAction(QStringLiteral("De&lete Layer"));
    connect(remove, &QAction::triggered, this, [this] { delete_layer(); });
    QAction* merge = layer->addAction(QStringLiteral("Mer&ge Down / Group"));
    merge->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    connect(merge, &QAction::triggered, this, [this] { merge_layers(); });
    layer->addSeparator();
    QMenu* mask_menu = layer->addMenu(QStringLiteral("Layer &Mask"));
    QAction* reveal = mask_menu->addAction(QStringLiteral("Reveal All"));
    connect(reveal, &QAction::triggered, this, [this] { add_mask(false, false); });
    QAction* hide = mask_menu->addAction(QStringLiteral("Hide All"));
    connect(hide, &QAction::triggered, this, [this] { add_mask(false, true); });
    QAction* from_selection = mask_menu->addAction(QStringLiteral("From Selection"));
    connect(from_selection, &QAction::triggered, this, [this] { add_mask(true, false); });
    mask_menu->addSeparator();
    QAction* invert_mask_item = mask_menu->addAction(QStringLiteral("Invert Mask"));
    connect(invert_mask_item, &QAction::triggered, this, [this] { invert_mask(); });
    QAction* toggle_mask_item = mask_menu->addAction(QStringLiteral("Toggle Mask"));
    connect(toggle_mask_item, &QAction::triggered, this, [this] { toggle_mask(); });
    QAction* apply_mask_item = mask_menu->addAction(QStringLiteral("Apply Mask"));
    connect(apply_mask_item, &QAction::triggered, this, [this] { apply_mask(); });
    QAction* delete_mask_item = mask_menu->addAction(QStringLiteral("Delete Mask"));
    connect(delete_mask_item, &QAction::triggered, this, [this] { remove_mask(); });
    QAction* clipping = layer->addAction(QStringLiteral("Create/Release Clipping Mask"));
    clipping->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+G")));
    connect(clipping, &QAction::triggered, this, [this] { toggle_clipping(); });
    layer->addSeparator();
    QAction* folder = layer->addAction(QStringLiteral("New &Folder"));
    connect(folder, &QAction::triggered, this, [this] { new_folder(); });
    QAction* rename = layer->addAction(QStringLiteral("&Rename Layer"));
    rename->setShortcut(QKeySequence(Qt::Key_F2));
    connect(rename, &QAction::triggered, this, [this] { rename_layer(); });
    layer->addSeparator();
    QAction* forward = layer->addAction(QStringLiteral("Bring &Forward"));
    connect(forward, &QAction::triggered, this, [this] { bring_forward(); });
    QAction* backward = layer->addAction(QStringLiteral("Send &Backward"));
    connect(backward, &QAction::triggered, this, [this] { send_backward(); });

    menuBar()->addMenu(QStringLiteral("&Window"));
    QMenu* help = menuBar()->addMenu(QStringLiteral("&Help"));
    QAction* digest_action = help->addAction(QStringLiteral("Project &Digest..."));
    connect(digest_action, &QAction::triggered, this, [this] { show_digest(); });
    QAction* about = help->addAction(QStringLiteral("About Compositor"));
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(this, QStringLiteral("Compositor"),
                           QStringLiteral("Compositor ???open-source image editor.\nCross-platform core (WIP)."));
    });

    // Every menu command that carries a shortcut is remappable.
    for (QAction* menuAction : menuBar()->actions()) {
        QMenu* menu = menuAction->menu();
        if (menu == nullptr) {
            continue;
        }
        for (QAction* action : menu->actions()) {
            if (!action->shortcut().isEmpty() && !action->text().isEmpty()) {
                QString label = action->text();
                label.remove(QLatin1Char('&'));
                register_action(QStringLiteral("menu.") + label, action);
            }
        }
    }
}

}  // namespace compositor::appwin
