#pragma once
#include <QColor>
#include <QImage>
#include <QString>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "compositor/model/project_store.hpp"
#include "compositor/render/adjustment.hpp"
#include "compositor/render/rgba_surface.hpp"
#include "compositor/render/selection.hpp"
#include "compositor/render/shape.hpp"
#include "compositor/render/warp.hpp"

namespace compositor::appwin {

/// The brush the user is painting with, in layer pixels.
struct BrushSettings {
    double radius = 12.0;
    double hardness = 0.7;
    double opacity = 1.0;
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    bool erase = false;
    bool blur = false;
};

/// One open `.comp`: its loaded snapshot, the flattened pixels, and the Qt image the canvas shows.
///
/// The session owns all model and rendering work; the widgets only ask it for pixels. Opening decodes
/// every layer, composites the document, and hands back any error text for the UI to show.
class DocumentSession {
public:
    bool open(const QString& path, QString& error);
    /// A new empty project: a transparent pixel layer over a `width`×`height` canvas.
    bool create(int width, int height, QString& error);
    bool save(const QString& path, QString& error);
    /// Export a flattened layered Photoshop file (name, rectangle, opacity, visibility, blend mode).
    bool export_psd(const QString& path, QString& error);

    /// Live, non-destructive filter preview for a docked panel: apply an adjustment to the layer's
    /// pixels and re-composite without an undo step, so the canvas updates as controls move; commit
    /// writes it as one undo step, cancel restores the original pixels.
    bool begin_filter_preview(const std::string& id, QString& error);
    bool update_filter_preview(const nlohmann::json& adjustment, QString& error);
    bool commit_filter_preview(QString& error);
    void cancel_filter_preview();
    [[nodiscard]] bool filter_preview_active() const { return preview_active_; }
    /// Re-read the project from its file, discarding in-memory edits (used when the file changed on
    /// disk). False when the project has never been saved.
    bool reload(QString& error);
    [[nodiscard]] const std::string& path() const { return path_; }
    bool export_png(const QString& path, QString& error);
    bool export_jpeg(const QString& path, int quality, QString& error);

    /// Show or hide a layer and re-composite. The change is kept in memory, so a later Save writes it.
    bool set_visibility(const std::string& id, bool visible, QString& error);

    /// Move a layer by a delta in document pixels, and set a layer's opacity (0–1). Both re-composite.
    bool move_layer(const std::string& id, double dx, double dy, QString& error);
    /// Set a layer's origin outright, without a history step (used while a snapped move is in progress).
    bool set_layer_origin(const std::string& id, double origin_x, double origin_y, QString& error);
    bool set_opacity(const std::string& id, double opacity, QString& error);
    bool set_blend_mode(const std::string& id, model::LayerBlendMode mode, QString& error);

    /// The record for a layer, or nullptr. For the UI to read a layer's current settings.
    [[nodiscard]] const model::ProjectLayerRecord* layer(const std::string& id) const;

    void set_brush(const BrushSettings& brush);

    /// Paint a stroke into a layer's own pixels. `stroke_begin`/`stroke_move` take document pixels
    /// (the canvas's coordinates) and map them into the layer; the pixels are re-encoded and the
    /// document re-composited each call. Each stroke is one undo step.
    bool stroke_begin(const std::string& id, double document_x, double document_y, QString& error);
    bool stroke_move(double document_x, double document_y, QString& error);
    void stroke_end();

    /// Smudge / Liquify on the active layer's pixels. `mode` is 0 Liquify, 1 Smudge. The stroke edits
    /// the layer's working pixels live and writes them back on `warp_end`, as one undo step.
    bool warp_begin(const std::string& id, double x, double y, int mode, QString& error);
    void warp_move(double x, double y);
    void warp_end();

    /// Distort: the active layer's four corners in document space (for the canvas handles), and a
    /// perspective warp moving them there — the pixels are resampled into the new shape.
    [[nodiscard]] bool layer_quad(const std::string& id, render::Quad& out) const;
    bool apply_distort(const std::string& id, const render::Quad& corners, QString& error);

    /// Layer operations. `active_id` (may be empty) is where a new layer goes.
    bool add_blank_layer(const std::string& active_id, QString& error);
    bool duplicate_layer(const std::string& id, QString& error);
    bool delete_layer(const std::string& id, QString& error);

    /// Layer transforms. Flip toggles a mirror; rotate turns about the center by 90 degrees; reset
    /// clears rotation and flips and centers the layer at its natural size.
    bool flip_layer(const std::string& id, bool horizontal, QString& error);
    bool rotate_layer_90(const std::string& id, bool clockwise, QString& error);
    bool reset_transform(const std::string& id, QString& error);
    /// Set a layer's transform wholesale (the Transform inspector): origin, size, rotation and flips.
    bool set_transform(const std::string& id, const model::LayerTransform& transform, QString& error);
    bool flip_canvas(bool horizontal, QString& error);
    bool rename_layer(const std::string& id, const QString& name, QString& error);
    /// Move a layer among its siblings: `delta` +1 brings it forward (up), -1 sends it backward.
    bool move_layer_order(const std::string& id, int delta, QString& error);
    bool add_folder(const std::string& active_id, QString& error);

    /// Merge every visible layer (adjustments and effects included) into one opaque layer.
    bool flatten(QString& error);

    /// ⌘E: merge the active layer with the one below it in the same folder, or — for a folder — merge
    /// its contents. The result is composited exactly as the canvas shows it (blend, opacity, mask,
    /// clipping and adjustments baked in), trimmed to what is there, and put back in place.
    bool merge_layers(QString& error);

    /// Drag in the Layers panel: set the manifest to the given top-first order (the panel's order).
    /// Only the stacking order changes; parent and clipping links are kept. One undo step.
    bool reorder_layers(const std::vector<std::string>& top_first_ids, QString& error);

    /// Drag in the Layers tree: each entry is a layer id and the folder it now lives in (nullopt at the
    /// root), top-first as the panel shows them. Parenting and sibling order are both set. One undo step.
    bool set_layer_tree(const std::vector<std::pair<std::string, std::optional<std::string>>>& top_first,
                        QString& error);

    /// Copy the active layer (a folder with everything it holds) and paste it back — in this project
    /// or another open one — as new layers above the active one, as Photoshop does.
    void copy_layers();
    [[nodiscard]] bool can_paste_layers() const { return clipboard_.has_value(); }
    bool paste_layers(QString& error);

    /// ⌘C: with a selection, copy the active layer's pixels through it; without one, copy the whole
    /// layer. ⌘⇧C (Copy Merged) composites every visible layer through the selection. ⌘V pastes pixels
    /// back where they came from as a new layer, or a copied layer as new layers. ⌘X cuts, ⌘J is
    /// Layer via Copy. Ported from the macOS app's `SelectionClipboard.swift`.
    bool copy_selection(QString& error);
    bool copy_merged(QString& error);
    bool cut_selection(QString& error);
    bool paste(QString& error);
    bool layer_via_copy(QString& error);
    /// Lift the selected pixels of the active layer onto a new layer above it (the source region is
    /// cleared), so the floating pixels can be moved, scaled and rotated before merging down.
    bool float_selection(QString& error);
    /// The most recently copied pixels as an image for the system clipboard, or a null image.
    [[nodiscard]] QImage pixel_clipboard_image() const;

    /// Adjustment layers and destructive filters. An adjustment layer carries an `adjustment` object
    /// (the kind plus its settings) and affects the canvas below it; a filter bakes the same effect
    /// into a pixel layer's own pixels. Both use the core `apply_adjustment`.
    bool add_adjustment_layer(const std::string& active_id, const std::string& kind, QString& error);
    /// Add a Levels adjustment layer above the active layer whose ranges come from its own pixels:
    /// 0 Contrast, 1 Color, 2 Color + neutral midtones (macOS's LevelsAutomatic).
    bool auto_levels(int mode, QString& error);
    /// The composited document's 4×256 histogram (RGB, red, green, blue) for the Levels panel.
    [[nodiscard]] std::array<double, 1024> histogram() const { return render::histogram(flat_); }

    /// Content-Aware Fill: synthesize the selected pixels of the active layer from the surrounding image.
    bool content_aware_fill(const std::string& id, QString& error);
    bool update_adjustment(const std::string& id, const nlohmann::json& adjustment, QString& error);
    /// Set a layer's `effects` object (stroke, shadow, glows, overlays). An empty object clears them.
    bool set_effects(const std::string& id, const nlohmann::json& effects, QString& error);

    /// Editable text layers. `style` follows the macOS app's `LayerTextStyle` schema (content,
    /// fontName, fontSize, red/green/blue, alignment, tracking, leading, optional `boxSize`). The
    /// rasterized PNG is the display and export fallback, so text round-trips with the macOS app.
    bool add_text_layer(const nlohmann::json& style, double origin_x, double origin_y, QString& error);
    bool update_text_layer(const std::string& id, const nlohmann::json& style, QString& error);

    /// A Shape layer: a new layer whose pixels are the shape, with `shape` metadata kept alongside so
    /// the shape stays editable and round-trips with the macOS app (as `LayerShapeStyle`).
    bool add_shape_layer(const render::ShapeStyle& style, double origin_x, double origin_y, int width, int height,
                         QString& error);
    /// Redraw a shape layer at its current size with new settings, keeping it editable (macOS redraws a
    /// shape when it is changed rather than baking it).
    bool update_shape_layer(const std::string& id, const render::ShapeStyle& style, QString& error);
    [[nodiscard]] bool is_shape_layer(const std::string& id) const;
    /// Redraw a shape layer's pixels at its transform's size from its stored style (used when the layer
    /// is scaled, so a rounded corner keeps its radius instead of stretching). No history of its own.
    bool redraw_shape_pixels(const std::string& id, QString& error);
    bool apply_filter(const std::string& id, const nlohmann::json& adjustment, QString& error);

    /// Spot Healing: the drag marks a region; on release the core heals it from its surroundings.
    bool heal_begin(const std::string& id, double document_x, double document_y, QString& error);
    void heal_move(double document_x, double document_y);
    void heal_end();

    /// Clone Stamp: set a source with `clone_set_source`, then drag to copy from it (aligned).
    bool clone_set_source(const std::string& id, double document_x, double document_y, QString& error);
    bool clone_begin(const std::string& id, double document_x, double document_y, QString& error);
    void clone_move(double document_x, double document_y);
    void clone_end();

    /// Paint a gradient (linear or radial) on a pixel layer, limited to the current selection if any.
    bool draw_gradient(const std::string& id, double a_red, double a_green, double a_blue, double a_alpha, double b_red,
                       double b_green, double b_blue, double b_alpha, double start_x, double start_y, double end_x,
                       double end_y, bool radial, QString& error);
    /// Fill a shape (a coverage selection) on a pixel layer.
    bool fill_shape(const std::string& id, const render::Selection& shape, double red, double green, double blue,
                    double opacity, QString& error);

    /// Canvas operations. `canvas_size` resizes the canvas (anchor 0–8, top-left to bottom-right) and
    /// shifts the layers; `image_size` scales every layer; `trim` crops to the visible content.
    bool canvas_size(int width, int height, int anchor, QString& error);
    bool image_size(int width, int height, QString& error);
    bool trim(QString& error);

    /// Alignment guides (non-printing). `horizontal` sits at a document Y, else a document X.
    bool add_guide(bool horizontal, double position, QString& error);
    bool clear_guides(QString& error);
    [[nodiscard]] const std::vector<model::CanvasGuide>* guides() const;

    /// Layer masks and clipping masks.
    bool add_layer_mask(const std::string& id, bool from_selection, bool invert, QString& error);
    bool remove_layer_mask(const std::string& id, QString& error);
    bool invert_layer_mask(const std::string& id, QString& error);
    bool toggle_layer_mask(const std::string& id, QString& error);
    bool apply_layer_mask(const std::string& id, QString& error);
    bool toggle_clipping_mask(const std::string& id, QString& error);

    /// The selection, in document pixels, and the operations it enables. Every tool produces a
    /// coverage mask, so they share one model. `mode` combines the new selection with the old one
    /// (Shift adds, Alt subtracts, Shift+Alt intersects), as Photoshop does.
    void select_rect(const render::DocRect& rect, render::CombineMode mode);
    void select_ellipse(const render::DocRect& bounds, render::CombineMode mode);
    void select_lasso(const std::vector<render::Point>& points, render::CombineMode mode);
    void clear_selection();
    [[nodiscard]] bool has_selection() const { return selection_ && !selection_->empty(); }
    [[nodiscard]] const std::optional<render::Selection>& selection() const { return selection_; }
    [[nodiscard]] std::vector<std::vector<render::Point>> selection_outline() const;
    void select_all();
    void invert_selection();
    /// Load a selection from a layer's own pixels (its ≥50%-opaque area) or its mask (the masked-out
    /// area), in document space, combined with the current selection by `mode` (macOS's MaskTracing).
    bool load_layer_selection(const std::string& id, render::CombineMode mode, QString& error);
    bool load_mask_selection(const std::string& id, render::CombineMode mode, QString& error);
    /// Select > Color Range: the pixels of the composited document near `red`/`green`/`blue` within
    /// `fuzziness` (0–200), optionally inverted, combined with the current selection by `mode`.
    bool color_range_select(std::uint8_t red, std::uint8_t green, std::uint8_t blue, int fuzziness, bool invert,
                            render::CombineMode mode, QString& error);
    /// Refine the current selection's edges against the composite (macOS's GuidedMatte): soft, hair-like
    /// detail follows the image's own edges. `radius` is in document pixels.
    bool refine_selection_edges(int radius, QString& error);
    void feather_selection(double radius);
    void expand_selection(int radius);
    void contract_selection(int radius);
    bool select_wand(const std::string& id, double document_x, double document_y, int tolerance, bool contiguous,
                     render::CombineMode mode, QString& error);

    /// The composited color under a document point, for the eyedropper. Null when nothing is open.
    [[nodiscard]] QColor sample_color(double document_x, double document_y) const;
    bool delete_selection(const std::string& id, QString& error);
    bool fill_selection(const std::string& id, QString& error);
    bool crop_to_selection(QString& error);

    /// Import a PNG or JPEG as a new layer (or, with no project open, as a new document sized to the
    /// image). The image is placed at the top-left of the canvas at its natural size.
    bool import_image(const QString& path, const std::string& active_id, QString& error);

    /// Import an 8-bit RGB Photoshop file as a new document (layers, folders and masks preserved).
    bool import_psd(const QString& path, QString& error);

    /// A snapshot-based history. `begin_interaction` records the state before a drag, so a whole move
    /// or opacity drag is one undo step; discrete edits record themselves.
    void begin_interaction();
    bool undo(QString& error);
    bool redo(QString& error);
    [[nodiscard]] bool can_undo() const { return !undo_history_.empty(); }
    [[nodiscard]] bool can_redo() const { return !redo_history_.empty(); }

    /// The History panel (macOS's DocumentHistory): each undoable edit is named, and any earlier or
    /// later state can be restored by index. State 0 is the document as opened.
    void set_edit_name(const std::string& name) { pending_edit_name_ = name; }
    [[nodiscard]] int history_current() const { return static_cast<int>(undo_history_.size()); }
    [[nodiscard]] int history_count() const { return static_cast<int>(undo_history_.size() + redo_history_.size()); }
    /// One label per state, oldest first, so a list can be shown with the current state highlighted.
    [[nodiscard]] std::vector<std::string> history_labels() const;
    bool jump_history(int state, QString& error);

    /// A short human-readable summary of the open project (macOS's ProjectDigest).
    [[nodiscard]] QString digest() const;

    [[nodiscard]] bool is_open() const { return snapshot_.has_value(); }
    [[nodiscard]] const QImage& image() const { return image_; }
    [[nodiscard]] const model::ProjectManifest* manifest() const;

    /// A small thumbnail of a layer's pixels for the Layers panel, or a null image for a folder or
    /// a layer with no pixels.
    [[nodiscard]] QImage layer_thumbnail(const std::string& id, int max_size) const;

private:
    bool recomposite(QString& error);
    void push_history();
    /// Keep decoded layer pixels in memory so re-compositing never decodes a PNG. New assets are
    /// decoded on demand; a stroke edits the cached surface directly.
    void ensure_surfaces();
    void invalidate_surfaces();
    void apply_selection(const render::Selection& incoming, render::CombineMode mode);

    std::optional<model::ProjectSnapshot> snapshot_;
    render::RgbaSurface flat_;
    QImage image_;

    BrushSettings brush_;
    bool stroking_ = false;
    std::string stroke_layer_;
    double last_x_ = 0.0;
    double last_y_ = 0.0;

    bool healing_ = false;
    std::string heal_layer_;
    std::vector<std::uint8_t> heal_coverage_;
    double heal_last_x_ = 0.0;
    double heal_last_y_ = 0.0;

    std::string path_;

    bool preview_active_ = false;
    std::string preview_layer_;
    render::RgbaSurface preview_original_;

    std::unique_ptr<render::WarpStroke> warp_;
    std::string warp_layer_;

    bool cloning_ = false;
    bool clone_has_source_ = false;
    std::string clone_layer_;
    double clone_source_x_ = 0.0;
    double clone_source_y_ = 0.0;
    double clone_offset_x_ = 0.0;
    double clone_offset_y_ = 0.0;
    double clone_last_x_ = 0.0;
    double clone_last_y_ = 0.0;

    std::map<std::string, render::RgbaSurface> image_surfaces_;
    std::map<std::string, render::RgbaSurface> mask_surfaces_;

    /// A whole layer (a folder with all it holds) kept between Copy and Paste, so it can come back in
    /// this project or another open one. IDs are reassigned when it is pasted.
    struct Clipboard {
        std::vector<model::ProjectLayerRecord> layers;
        std::map<std::string, model::ImageAsset> images;
        std::map<std::string, model::ImageAsset> masks;
    };

    /// Pixels copied through a selection (or the whole canvas), with where they came from so Paste puts
    /// them back in place.
    struct PixelClipboard {
        render::RgbaSurface surface;
        int origin_x = 0;
        int origin_y = 0;
    };

    /// Render the active layer (or, for `merged`, every visible layer) through the selection into a
    /// trimmed sRGB surface, and report where that surface sits on the canvas.
    bool render_selected_pixels(bool merged, render::RgbaSurface& surface, int& origin_x, int& origin_y,
                                QString& error);
    /// Add pixels as a new layer above the active layer (inside its folder), as one undo step.
    bool insert_pixel_layer(render::RgbaSurface surface, int origin_x, int origin_y, const char* name, QString& error);

    /// Photoshop's clipping after a layer moves: a dropped layer adopts a clipping stack it lands in,
    /// and any clipping left dangling by the move is released. Ported from the macOS app.
    void adopt_clipping(const std::string& id);
    void release_detached_clipping();

    /// Rasterize a `LayerTextStyle` into a premultiplied sRGB surface, using Qt where the macOS app
    /// uses CoreText. Shared by adding and editing text layers.
    bool rasterize_text(const nlohmann::json& style, render::RgbaSurface& surface, QString& error);

    std::vector<model::ProjectSnapshot> undo_history_;
    std::vector<model::ProjectSnapshot> redo_history_;
    std::vector<std::string> undo_names_;
    std::vector<std::string> redo_names_;
    std::string pending_edit_name_ = "Edit";
    std::optional<render::Selection> selection_;
    std::optional<Clipboard> clipboard_;
    std::optional<PixelClipboard> pixel_clipboard_;
};

}  // namespace compositor::appwin
