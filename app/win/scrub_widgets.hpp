#pragma once
#include <QDoubleSpinBox>
#include <QSlider>

namespace compositor::appwin {

/// A spin box whose value can be dragged: press and drag vertically to scrub, as the macOS app's
/// `NumericScrub` does. A plain click still edits or steps as usual.
class ScrubSpinBox : public QDoubleSpinBox {
public:
    explicit ScrubSpinBox(QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool scrub_ = false;
    bool pressed_ = false;
    QPoint press_pos_;
    double start_value_ = 0.0;
};

/// A slider that settles on its default when released near it, as the macOS app's `SliderSnap` does.
class SnapSlider : public QSlider {
public:
    explicit SnapSlider(Qt::Orientation orientation, QWidget* parent = nullptr);

    void set_snap(int default_value, int threshold = 4);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    int snap_value_ = -1;
    int threshold_ = 4;
};

}  // namespace compositor::appwin
