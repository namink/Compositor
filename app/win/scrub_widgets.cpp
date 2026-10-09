#include "scrub_widgets.hpp"

#include <QLineEdit>
#include <QMouseEvent>
#include <cmath>

namespace compositor::appwin {

ScrubSpinBox::ScrubSpinBox(QWidget* parent) : QDoubleSpinBox(parent) {
    setButtonSymbols(QAbstractSpinBox::NoButtons);
    setKeyboardTracking(false);
}

void ScrubSpinBox::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !lineEdit()->isReadOnly()) {
        pressed_ = true;
        scrub_ = false;
        press_pos_ = event->pos();
        start_value_ = value();
        event->accept();
        return;
    }
    QDoubleSpinBox::mousePressEvent(event);
}

void ScrubSpinBox::mouseMoveEvent(QMouseEvent* event) {
    if (!pressed_) {
        QDoubleSpinBox::mouseMoveEvent(event);
        return;
    }
    const int dy = press_pos_.y() - event->pos().y();
    if (!scrub_) {
        if (std::abs(dy) < 4) {
            return;
        }
        scrub_ = true;
        setCursor(Qt::SizeVerCursor);
    }
    setValue(start_value_ + dy * singleStep());
    event->accept();
}

void ScrubSpinBox::mouseReleaseEvent(QMouseEvent* event) {
    if (pressed_) {
        pressed_ = false;
        if (scrub_) {
            scrub_ = false;
            unsetCursor();
            event->accept();
            return;
        }
        // A plain click: let the base class put the cursor in the field for typing.
        setFocus(Qt::MouseFocusReason);
        selectAll();
        event->accept();
        return;
    }
    QDoubleSpinBox::mouseReleaseEvent(event);
}

SnapSlider::SnapSlider(Qt::Orientation orientation, QWidget* parent) : QSlider(orientation, parent) {}

void SnapSlider::set_snap(int default_value, int threshold) {
    snap_value_ = default_value;
    threshold_ = threshold;
}

void SnapSlider::mouseReleaseEvent(QMouseEvent* event) {
    QSlider::mouseReleaseEvent(event);
    if (snap_value_ >= 0 && std::abs(value() - snap_value_) <= threshold_) {
        setValue(snap_value_);
    }
}

}  // namespace compositor::appwin
