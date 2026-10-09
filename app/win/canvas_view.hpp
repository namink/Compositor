#pragma once
#include <QImage>
#include <QOpenGLWidget>
#include <QPointF>
#include <QRectF>
#include <functional>
#include <utility>
#include <vector>

class QMouseEvent;
class QPainter;
class QTimer;
class QWheelEvent;

namespace compositor::appwin {

/// Shows a flattened document, scaled to fit, over a checkerboard for transparency.
///
/// Rendered on the GPU through Qt's OpenGL paint engine (`QOpenGLWidget`): the composited image is
/// uploaded once and the checkerboard, image and selection outline are drawn with a GPU transform.
/// The heavy compositing is still the CPU core; a Skia or `QRhi` backend can replace this widget
/// behind the same interface later.
class CanvasView : public QOpenGLWidget {
public:
    explicit CanvasView(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    /// Replace the pixels but keep the current zoom and pan (for edits that re-composite).
    void updateImage(const QImage& image);
    void clear();

    void zoom_in();
    void zoom_out();
    void zoom_fit();
    void zoom_actual();
    /// Frame pixels per document pixel at the current zoom (for snapping tolerances).
    [[nodiscard]] double scale() const;

    /// In move mode, dragging reports its delta in document pixels to `handler` instead of panning.
    void set_move_mode(bool move);
    void set_move_handler(std::function<void(double, double)> handler);
    /// Called once when a move drag begins, so the caller can snapshot for undo.
    void set_move_begin_handler(std::function<void()> handler);

    /// In paint mode, dragging reports document-pixel positions to the begin/move/end handlers.
    void set_paint_mode(bool paint);
    void set_paint_handlers(std::function<void(double, double)> begin, std::function<void(double, double)> move,
                            std::function<void()> end);

    /// In rectangle-select mode, dragging builds a document-space rectangle.
    void set_select_mode(bool select);
    void set_selection_handler(std::function<void(double, double, double, double)> handler);

    /// In wand mode, a click reports its document position.
    void set_wand_mode(bool wand);
    void set_wand_handler(std::function<void(double, double)> handler);

    /// In lasso mode, a drag collects document-space points and reports them on release.
    void set_lasso_mode(bool lasso);
    void set_lasso_handler(std::function<void(const std::vector<QPointF>&)> handler);

    /// In polygon-lasso mode, clicks add corners; a double-click closes and reports the polygon.
    void set_polygon_mode(bool polygon);

    /// In eyedropper mode, a click reports its document position.
    void set_eyedropper_mode(bool eyedropper);
    void set_eyedropper_handler(std::function<void(double, double)> handler);

    /// In gradient mode, a drag reports its start and end document points on release.
    void set_gradient_mode(bool gradient);
    void set_gradient_handler(std::function<void(double, double, double, double)> handler);

    /// In heal mode, a drag reports document positions to begin/move/end, like painting.
    void set_heal_mode(bool heal);
    void set_heal_handlers(std::function<void(double, double)> begin, std::function<void(double, double)> move,
                           std::function<void()> end);

    /// In clone mode, Alt-drag (once) sets the source; a plain drag copies from it.
    void set_clone_mode(bool clone);
    void set_clone_handlers(std::function<void(double, double)> source, std::function<void(double, double)> begin,
                            std::function<void(double, double)> move, std::function<void()> end);

    /// In crop mode, a drag reports a document-space rectangle on release.
    void set_crop_mode(bool crop);
    void set_crop_handler(std::function<void(double, double, double, double)> handler);

    /// In text mode, a click reports the document point to drop new text at.
    void set_text_mode(bool text);
    void set_text_handler(std::function<void(double, double)> handler);

    /// The brush cursor circle: its diameter in document pixels, drawn where the pointer hovers while
    /// a brush-like tool is active (macOS's BrushCursorOverlay).
    void set_brush_diameter(double diameter);

    /// Rulers along the top and left edges, showing document coordinates; dragging out of one creates
    /// an alignment guide (macOS's CanvasRulers).
    void set_rulers_visible(bool visible);
    void set_guide_handler(std::function<void(bool horizontal, double position)> handler);

    /// Called whenever the shown image changes, so the History panel can refresh.
    void set_image_changed_handler(std::function<void()> handler);

    /// View state for the navigator: the current pan and scale, the visible area size, and the
    /// document size, plus a handler called whenever the view changes.
    /// Map a document point to a widget point and report the current zoom, for the inline text editor.
    [[nodiscard]] QPointF document_to_widget(const QPointF& document) const { return to_widget(document); }
    [[nodiscard]] double view_scale() const { return current_scale(); }

    void set_pan(const QPointF& pan);
    [[nodiscard]] QPointF pan() const { return pan_; }
    [[nodiscard]] QSize document_size() const { return image_.size(); }
    [[nodiscard]] QSize viewport_size() const;
    void set_view_changed_handler(std::function<void()> handler);

    /// Distort: four draggable corner handles over the active layer, reporting the moved corners
    /// (document space, top-left/top-right/bottom-right/bottom-left) so the layer can be warped.
    void set_distort_mode(bool distort);
    void set_distort_quad(const std::vector<QPointF>& corners);
    void set_distort_handler(std::function<void(const std::vector<QPointF>&)> handler);

    /// The combine mode for the selection being made (0 replace, 1 add, 2 subtract, 3 intersect),
    /// from the modifier keys held when the drag or click began.
    [[nodiscard]] int selection_mode() const { return selection_mode_; }

    /// The selection's outline loops, in document pixels, drawn as dashed lines.
    void set_selection_outline(std::vector<std::vector<QPointF>> loops);

    /// Non-printing overlays: alignment guides (axis 0 horizontal, 1 vertical; document pixels) and a
    /// layout grid.
    void set_guides(std::vector<std::pair<int, double>> guides);
    void set_grid(bool visible, int spacing, int subdivisions);

protected:
    void paintGL() override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] double fit_scale() const;
    [[nodiscard]] double current_scale() const;
    [[nodiscard]] QPointF to_document(const QPointF& widget) const;
    [[nodiscard]] QPointF to_widget(const QPointF& document) const;
    [[nodiscard]] QRectF viewport_rect() const;
    [[nodiscard]] double ruler_thickness() const;
    void draw_rulers(QPainter& painter);
    void notify_view_changed();
    void update_cursor();
    void draw_overlays(QPainter& painter);
    void flush_stroke_move();

    QImage image_;
    double zoom_ = 1.0;
    bool fit_ = true;
    QPointF pan_;
    bool panning_ = false;
    bool move_mode_ = false;
    bool moving_ = false;
    bool paint_mode_ = false;
    bool painting_ = false;
    bool select_mode_ = false;
    bool selecting_ = false;
    bool wand_mode_ = false;
    bool lasso_mode_ = false;
    bool lassoing_ = false;
    bool polygon_mode_ = false;
    bool eyedropper_mode_ = false;
    bool gradient_mode_ = false;
    bool gradienting_ = false;
    bool heal_mode_ = false;
    bool healing_ = false;
    bool clone_mode_ = false;
    bool cloning_ = false;
    bool crop_mode_ = false;
    bool text_mode_ = false;

    double brush_diameter_ = 0.0;
    bool hovering_ = false;
    QPointF hover_pos_;

    bool distort_mode_ = false;
    std::vector<QPointF> distort_quad_;
    std::function<void(const std::vector<QPointF>&)> distort_handler_;
    int distort_corner_ = -1;
    bool distorting_ = false;

    bool rulers_visible_ = false;
    std::function<void(bool, double)> guide_handler_;
    std::function<void()> image_changed_handler_;
    std::function<void()> view_changed_handler_;
    bool dragging_guide_ = false;
    bool guide_horizontal_ = false;
    double guide_position_ = 0.0;
    bool croping_ = false;
    QPointF crop_anchor_;
    QRectF crop_rect_;
    int selection_mode_ = 0;
    QPointF select_anchor_;
    QPointF gradient_start_;
    QPointF gradient_end_;
    QPointF last_mouse_;
    QTimer* stroke_timer_ = nullptr;
    int stroke_pending_mode_ = 0;  // 1 paint, 2 heal, 3 clone
    QPointF stroke_pending_pos_;
    bool stroke_pending_ = false;
    std::vector<QPointF> lasso_points_;
    std::vector<QPointF> polygon_points_;
    std::vector<std::vector<QPointF>> selection_outline_;
    std::vector<std::pair<int, double>> guides_;
    bool grid_visible_ = false;
    int grid_spacing_ = 64;
    int grid_subdivisions_ = 8;
    std::function<void(double, double)> move_handler_;
    std::function<void()> move_begin_;
    std::function<void(double, double)> paint_begin_;
    std::function<void(double, double)> paint_move_;
    std::function<void()> paint_end_;
    std::function<void(double, double, double, double)> selection_handler_;
    std::function<void(double, double)> wand_handler_;
    std::function<void(const std::vector<QPointF>&)> lasso_handler_;
    std::function<void(double, double)> eyedropper_handler_;
    std::function<void(double, double, double, double)> gradient_handler_;
    std::function<void(double, double)> text_handler_;
    std::function<void(double, double)> heal_begin_;
    std::function<void(double, double)> heal_move_;
    std::function<void()> heal_end_;
    std::function<void(double, double)> clone_source_;
    std::function<void(double, double)> clone_begin_;
    std::function<void(double, double)> clone_move_;
    std::function<void()> clone_end_;
    std::function<void(double, double, double, double)> crop_handler_;
};

}  // namespace compositor::appwin
