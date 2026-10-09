#pragma once
#include <QColor>
#include <QHash>
#include <QMainWindow>
#include <memory>
#include <vector>

#include "document_session.hpp"

class QAction;
class QCheckBox;
class QDockWidget;
class QDragEnterEvent;
class QEvent;
class QDropEvent;
class QFileSystemWatcher;
class QMenu;
class QPlainTextEdit;

namespace compositor::appwin {
class ColorSwatch;
class NavigatorWidget;
class ToolDock;
class ScrubSpinBox;
class SnapSlider;
}  // namespace compositor::appwin
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QListWidget;
class QSlider;
class QTabBar;
class QTreeWidget;
class QTreeWidgetItem;

namespace compositor::appwin {

class CanvasView;

/// The desktop window, laid out like the macOS app: a vertical tool strip on the left, a tool-options
/// bar across the top, the canvas in the middle, and a Layers panel on the right with a blend-mode
/// picker, an opacity slider and a thumbnail list.
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void build_menus();
    void build_tool_toolbar();
    void build_options_toolbar();
    void build_layers_dock();
    void build_history_dock();
    void build_transform_dock();
    void build_palette_dock();
    void build_navigator_dock();
    void refresh_navigator();
    void refresh_transform();
    void apply_transform();
    void build_tabs();
    void wire_canvas();
    void refresh_distort_quad();
    void apply_move_snap(double dx, double dy);
    void register_document(std::unique_ptr<DocumentSession> document);
    void register_action(const QString& id, QAction* action);
    void load_shortcuts();
    void show_shortcuts_dialog();
    void switch_document(int index);
    void close_document(int index);
    void show_active_document();
    void open_project(const QString& path);
    void save_project(const QString& path);
    void new_project();
    void open_recent(const QString& path);
    void add_recent(const QString& path);
    void rebuild_recent_menu();
    void show_digest();
    void watch_active_document();
    void on_file_changed(const QString& path);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QMenu* recent_menu_ = nullptr;
    QFileSystemWatcher* watcher_ = nullptr;
    QListWidget* palette_ = nullptr;
    NavigatorWidget* navigator_ = nullptr;
    QLabel* layer_count_ = nullptr;
    ToolDock* tool_dock_ = nullptr;
    QDockWidget* history_dock_ = nullptr;
    QDockWidget* transform_dock_ = nullptr;
    QDockWidget* navigator_dock_ = nullptr;
    QDockWidget* palette_dock_ = nullptr;
    ColorSwatch* color_swatch_ = nullptr;
    QWidget* canvas_overlay_ = nullptr;
    QColor background_color_{255, 255, 255};
    nlohmann::json editing_filter_;
    QPlainTextEdit* text_editor_ = nullptr;
    std::string editing_text_id_;
    nlohmann::json editing_text_style_;
    bool editing_text_new_ = false;
    QString watched_path_;
    qint64 watched_mtime_ = 0;
    void export_png(const QString& path);
    void import_image();
    void import_psd();
    void refresh_layers();
    void refresh_history();
    void on_history_clicked();
    void on_layer_item_changed(QTreeWidgetItem* item);
    void on_layer_selected();
    void on_layers_reordered();
    void on_opacity_changed(int percent);
    void on_blend_changed(int index);
    void on_brush_changed();
    void choose_brush_color();
    void undo();
    void redo();
    void export_jpeg_menu();
    void export_psd_menu();
    void copy_merged();
    void copy_selection();
    void cut_selection();
    void paste();
    void layer_via_copy();
    void add_layer();
    void duplicate_layer();
    void delete_layer();
    void new_folder();
    void rename_layer();
    void bring_forward();
    void send_backward();
    void layer_effects();
    void edit_shape();
    void text_tool_click(double document_x, double document_y);
    void begin_text_edit(const std::string& id, bool is_new);
    void update_text_edit();
    void finish_text_edit();
    void new_text_layer();
    void edit_text();
    void text_runs();
    void new_adjustment_layer(const QString& kind);
    void auto_levels(int mode);
    void filter_layer(const QString& kind);
    void camera_raw_color();
    void edit_adjustment();
    void canvas_size_menu();
    void image_size_menu();
    void trim_canvas();
    void toggle_grid(bool visible);
    void toggle_rulers(bool visible);
    void grid_settings();
    void new_guide();
    void clear_guides();
    void refresh_overlays();
    void add_mask(bool from_selection, bool invert);
    void remove_mask();
    void invert_mask();
    void toggle_mask();
    void apply_mask();
    void toggle_clipping();
    void flip_canvas_horizontal();
    void flip_canvas_vertical();
    void rotate_canvas_cw();
    void rotate_canvas_ccw();
    void flip_horizontal();
    void flip_vertical();
    void rotate_cw();
    void rotate_ccw();
    void reset_transform();
    void flatten();
    void merge_layers();
    void delete_selection();
    void fill_selection();
    void content_aware_fill();
    void float_selection();
    void select_all();
    void deselect();
    void invert_selection();
    void load_layer_selection();
    void load_mask_selection();
    void color_range();
    void refine_edge();
    void grow_selection();
    void shrink_selection();
    void feather_selection();
    void crop_to_selection();
    void refresh_selection_outline();
    void report(const QString& error);

    DocumentSession placeholder_;
    DocumentSession* session_ = &placeholder_;
    std::vector<std::unique_ptr<DocumentSession>> documents_;
    QHash<QString, QAction*> shortcut_actions_;
    QTabBar* tabs_ = nullptr;
    CanvasView* canvas_ = nullptr;
    QTreeWidget* layers_ = nullptr;
    QListWidget* history_ = nullptr;
    QDoubleSpinBox* tr_x_ = nullptr;
    QDoubleSpinBox* tr_y_ = nullptr;
    QDoubleSpinBox* tr_w_ = nullptr;
    QDoubleSpinBox* tr_h_ = nullptr;
    QDoubleSpinBox* tr_rotation_ = nullptr;
    QCheckBox* tr_flip_x_ = nullptr;
    QCheckBox* tr_flip_y_ = nullptr;
    bool populating_transform_ = false;
    QComboBox* blend_combo_ = nullptr;
    QSlider* opacity_slider_ = nullptr;
    QLabel* opacity_value_ = nullptr;
    ScrubSpinBox* brush_size_ = nullptr;
    ScrubSpinBox* wand_tolerance_ = nullptr;
    ScrubSpinBox* shape_corner_ = nullptr;
    SnapSlider* brush_hardness_ = nullptr;
    SnapSlider* brush_opacity_ = nullptr;
    QLabel* hardness_value_ = nullptr;
    QLabel* brush_opacity_value_ = nullptr;
    QLabel* doc_info_ = nullptr;
    QLabel* options_title_ = nullptr;
    QWidget* brush_options_ = nullptr;
    QWidget* shape_options_ = nullptr;
    QWidget* wand_options_ = nullptr;
    QWidget* blur_options_ = nullptr;
    bool brush_erase_ = false;
    QPushButton* color_button_ = nullptr;
    QColor brush_color_ = Qt::black;
    int current_document_ = -1;
    QString selected_id_;
    /// Person-level tool options (macOS's ToolDefaults) kept in QSettings across tabs and launches.
    void load_tool_defaults();
    void save_tool_defaults();

    bool populating_layers_ = false;
    bool populating_history_ = false;
    bool reordering_layers_ = false;
    bool defaults_loaded_ = false;
    bool opacity_edit_started_ = false;
    bool ellipse_tool_ = false;
    bool blur_tool_ = false;
    int blur_mode_ = 0;   // 0 blur, 1 liquify, 2 smudge
    int shape_mode_ = 0;  // 0 none, 1 rectangle, 2 ellipse
    bool distort_tool_ = false;
    double shape_corner_radius_ = 0.0;
    bool shows_grid_ = false;
    bool shows_rulers_ = true;
    int grid_spacing_ = 64;
    int grid_subdivisions_ = 8;
    model::LayerTransform move_base_;
    double move_accum_x_ = 0.0;
    double move_accum_y_ = 0.0;
};

}  // namespace compositor::appwin
