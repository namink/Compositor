#include <QColor>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSlider>

#include "main_window.hpp"
#include "scrub_widgets.hpp"
#include "tool_defaults.hpp"

// Person-level tool options, loaded once and saved as they change, matching the macOS app's
// `ToolDefaults`: the brush options, wand tolerance, shape corner radius, brush color and grid look.

namespace compositor::appwin {

void MainWindow::load_tool_defaults() {
    if (brush_size_ != nullptr) {
        brush_size_->setValue(tool_double(QStringLiteral("brush.size"), brush_size_->value()));
    }
    if (brush_hardness_ != nullptr) {
        brush_hardness_->setValue(tool_int(QStringLiteral("brush.hardness"), brush_hardness_->value()));
    }
    if (brush_opacity_ != nullptr) {
        brush_opacity_->setValue(tool_int(QStringLiteral("brush.opacity"), brush_opacity_->value()));
    }
    if (wand_tolerance_ != nullptr) {
        wand_tolerance_->setValue(tool_double(QStringLiteral("wand.tolerance"), wand_tolerance_->value()));
    }
    if (shape_corner_ != nullptr) {
        shape_corner_->setValue(tool_double(QStringLiteral("shape.corner"), shape_corner_->value()));
    }
    const QString color = tool_string(QStringLiteral("brush.color"), QString());
    if (!color.isEmpty()) {
        const QColor stored(color);
        if (stored.isValid()) {
            brush_color_ = stored;
            if (color_button_ != nullptr) {
                color_button_->setStyleSheet(QStringLiteral("background-color: %1").arg(brush_color_.name()));
            }
        }
    }
    shows_grid_ = tool_bool(QStringLiteral("view.grid"), shows_grid_);
    grid_spacing_ = tool_int(QStringLiteral("view.gridSpacing"), grid_spacing_);
    grid_subdivisions_ = tool_int(QStringLiteral("view.gridSubdivisions"), grid_subdivisions_);
    shows_rulers_ = tool_bool(QStringLiteral("view.rulers"), shows_rulers_);
    on_brush_changed();
    defaults_loaded_ = true;
}

void MainWindow::save_tool_defaults() {
    if (!defaults_loaded_) {
        return;
    }
    if (brush_size_ != nullptr) {
        set_tool_double(QStringLiteral("brush.size"), brush_size_->value());
    }
    if (brush_hardness_ != nullptr) {
        set_tool_int(QStringLiteral("brush.hardness"), brush_hardness_->value());
    }
    if (brush_opacity_ != nullptr) {
        set_tool_int(QStringLiteral("brush.opacity"), brush_opacity_->value());
    }
    if (wand_tolerance_ != nullptr) {
        set_tool_double(QStringLiteral("wand.tolerance"), wand_tolerance_->value());
    }
    if (shape_corner_ != nullptr) {
        set_tool_double(QStringLiteral("shape.corner"), shape_corner_->value());
    }
    set_tool_string(QStringLiteral("brush.color"), brush_color_.name());
    set_tool_bool(QStringLiteral("view.grid"), shows_grid_);
    set_tool_int(QStringLiteral("view.gridSpacing"), grid_spacing_);
    set_tool_int(QStringLiteral("view.gridSubdivisions"), grid_subdivisions_);
    set_tool_bool(QStringLiteral("view.rulers"), shows_rulers_);
}

}  // namespace compositor::appwin
