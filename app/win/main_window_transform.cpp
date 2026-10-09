#include <QCheckBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "canvas_view.hpp"
#include "main_window.hpp"

// The Transform inspector (macOS's TransformInspector): the active layer's numeric transform, applied
// as a whole. Kept in its own file to stay within the size limit.

namespace compositor::appwin {

void MainWindow::build_transform_dock() {
    auto* dock = new QDockWidget(QStringLiteral("Transform"), this);
    auto* panel = new QWidget(dock);
    auto* layout = new QVBoxLayout(panel);
    auto* form = new QFormLayout();

    const auto add_spin = [&](const QString& label, double minimum, double maximum) {
        auto* spin = new QDoubleSpinBox(panel);
        spin->setRange(minimum, maximum);
        spin->setDecimals(2);
        form->addRow(label, spin);
        return spin;
    };
    tr_x_ = add_spin(QStringLiteral("X"), -1000000.0, 1000000.0);
    tr_y_ = add_spin(QStringLiteral("Y"), -1000000.0, 1000000.0);
    tr_w_ = add_spin(QStringLiteral("Width"), 1.0, 30000.0);
    tr_h_ = add_spin(QStringLiteral("Height"), 1.0, 30000.0);
    tr_rotation_ = add_spin(QStringLiteral("Rotation"), -360.0, 360.0);

    tr_flip_x_ = new QCheckBox(QStringLiteral("Flip horizontal"), panel);
    tr_flip_y_ = new QCheckBox(QStringLiteral("Flip vertical"), panel);
    form->addRow(QString(), tr_flip_x_);
    form->addRow(QString(), tr_flip_y_);

    layout->addLayout(form);
    auto* apply = new QPushButton(QStringLiteral("Apply Transform"), panel);
    connect(apply, &QPushButton::clicked, this, [this] { apply_transform(); });
    layout->addWidget(apply);
    layout->addStretch(1);

    dock->setWidget(panel);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    transform_dock_ = dock;
}

void MainWindow::refresh_transform() {
    if (tr_x_ == nullptr) {
        return;
    }
    const model::ProjectLayerRecord* record =
        selected_id_.isEmpty() ? nullptr : session_->layer(selected_id_.toStdString());
    populating_transform_ = true;
    if (record != nullptr) {
        tr_x_->setValue(record->transform.origin_x);
        tr_y_->setValue(record->transform.origin_y);
        tr_w_->setValue(record->transform.width);
        tr_h_->setValue(record->transform.height);
        tr_rotation_->setValue(record->transform.rotation);
        tr_flip_x_->setChecked(record->transform.flip_x);
        tr_flip_y_->setChecked(record->transform.flip_y);
    }
    const bool enabled = record != nullptr && !record->is_group_layer() && !record->adjustment;
    tr_x_->setEnabled(enabled);
    tr_y_->setEnabled(enabled);
    tr_w_->setEnabled(enabled);
    tr_h_->setEnabled(enabled);
    tr_rotation_->setEnabled(enabled);
    tr_flip_x_->setEnabled(enabled);
    tr_flip_y_->setEnabled(enabled);
    populating_transform_ = false;
}

void MainWindow::apply_transform() {
    if (selected_id_.isEmpty() || populating_transform_) {
        return;
    }
    model::LayerTransform transform;
    transform.origin_x = tr_x_->value();
    transform.origin_y = tr_y_->value();
    transform.width = tr_w_->value();
    transform.height = tr_h_->value();
    transform.rotation = tr_rotation_->value();
    transform.flip_x = tr_flip_x_->isChecked();
    transform.flip_y = tr_flip_y_->isChecked();
    session_->set_edit_name("Transform");
    QString error;
    if (!session_->set_transform(selected_id_.toStdString(), transform, error)) {
        report(error);
        return;
    }
    canvas_->updateImage(session_->image());
}

}  // namespace compositor::appwin
