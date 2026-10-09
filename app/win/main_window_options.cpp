#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

#include "main_window.hpp"
#include "scrub_widgets.hpp"

// The tool options bar (the macOS app's tool header): the tool's name and its controls, grouped so
// only the active tool's options show. Split out so `main_window.cpp` stays within the soft size limit.

namespace compositor::appwin {

void MainWindow::build_options_toolbar() {
    auto* options = new QToolBar(QStringLiteral("Options"), this);
    options->setObjectName(QStringLiteral("optionsBar"));
    options->setMovable(false);
    options->setFixedHeight(42);  // The macOS tool header bar's height.
    addToolBar(Qt::TopToolBarArea, options);

    options_title_ = new QLabel(QStringLiteral("Brush"), options);
    QFont title_font = options_title_->font();
    title_font.setPointSizeF(10.5);
    title_font.setBold(true);
    options_title_->setFont(title_font);
    options_title_->setMinimumWidth(66);
    options->addWidget(options_title_);

    // Brush: Paint/Erase, Size, Hardness, Opacity, Color.
    brush_options_ = new QWidget(options);
    auto* brush = new QHBoxLayout(brush_options_);
    brush->setContentsMargins(0, 0, 0, 0);
    brush->setSpacing(6);
    auto* brush_modes = new QWidget(brush_options_);
    auto* brush_modes_layout = new QHBoxLayout(brush_modes);
    brush_modes_layout->setContentsMargins(0, 0, 0, 0);
    brush_modes_layout->setSpacing(0);
    auto* paint_button = new QToolButton(brush_modes);
    paint_button->setText(QStringLiteral("Paint"));
    paint_button->setCheckable(true);
    paint_button->setChecked(true);
    auto* erase_button = new QToolButton(brush_modes);
    erase_button->setText(QStringLiteral("Erase"));
    erase_button->setCheckable(true);
    auto* brush_group = new QButtonGroup(brush_modes);
    brush_group->setExclusive(true);
    brush_group->addButton(paint_button, 0);
    brush_group->addButton(erase_button, 1);
    brush_modes_layout->addWidget(paint_button);
    brush_modes_layout->addWidget(erase_button);
    connect(brush_group, &QButtonGroup::idClicked, this, [this](int id) {
        brush_erase_ = id == 1;
        on_brush_changed();
    });
    brush->addWidget(brush_modes);

    brush->addWidget(new QLabel(QStringLiteral("Size"), brush_options_));
    brush_size_ = new ScrubSpinBox(brush_options_);
    brush_size_->setRange(1.0, 500.0);
    brush_size_->setDecimals(0);
    brush_size_->setSuffix(QStringLiteral(" px"));
    brush_size_->setValue(24.0);
    brush_size_->setMaximumWidth(80);
    brush->addWidget(brush_size_);
    connect(brush_size_, &QDoubleSpinBox::valueChanged, this, [this](double) { on_brush_changed(); });

    brush->addWidget(new QLabel(QStringLiteral("Hardness"), brush_options_));
    brush_hardness_ = new SnapSlider(Qt::Horizontal, brush_options_);
    brush_hardness_->setRange(0, 100);
    brush_hardness_->setValue(70);
    brush_hardness_->set_snap(70);
    brush_hardness_->setMaximumWidth(120);
    hardness_value_ = new QLabel(QStringLiteral("70 %"), brush_options_);
    connect(brush_hardness_, &QSlider::valueChanged, this, [this](int value) {
        hardness_value_->setText(QStringLiteral("%1 %").arg(value));
        on_brush_changed();
    });
    brush->addWidget(brush_hardness_);
    brush->addWidget(hardness_value_);

    brush->addWidget(new QLabel(QStringLiteral("Opacity"), brush_options_));
    brush_opacity_ = new SnapSlider(Qt::Horizontal, brush_options_);
    brush_opacity_->setRange(0, 100);
    brush_opacity_->setValue(100);
    brush_opacity_->set_snap(100);
    brush_opacity_->setMaximumWidth(120);
    brush_opacity_value_ = new QLabel(QStringLiteral("100 %"), brush_options_);
    connect(brush_opacity_, &QSlider::valueChanged, this, [this](int value) {
        brush_opacity_value_->setText(QStringLiteral("%1 %").arg(value));
        on_brush_changed();
    });
    brush->addWidget(brush_opacity_);
    brush->addWidget(brush_opacity_value_);

    brush->addWidget(new QLabel(QStringLiteral("Color"), brush_options_));
    color_button_ = new QPushButton(brush_options_);
    color_button_->setFixedSize(24, 24);
    color_button_->setToolTip(QStringLiteral("Brush color"));
    connect(color_button_, &QPushButton::clicked, this, [this] { choose_brush_color(); });
    brush->addWidget(color_button_);
    options->addWidget(brush_options_);

    // Shape: corner radius.
    shape_options_ = new QWidget(options);
    auto* shape = new QHBoxLayout(shape_options_);
    shape->setContentsMargins(0, 0, 0, 0);
    shape->setSpacing(6);
    shape->addWidget(new QLabel(QStringLiteral("Corner"), shape_options_));
    shape_corner_ = new ScrubSpinBox(shape_options_);
    shape_corner_->setRange(0.0, 2000.0);
    shape_corner_->setDecimals(0);
    shape_corner_->setSuffix(QStringLiteral(" px"));
    shape_corner_->setMaximumWidth(80);
    shape->addWidget(shape_corner_);
    connect(shape_corner_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        shape_corner_radius_ = value;
        save_tool_defaults();
    });
    options->addWidget(shape_options_);

    // Magic wand: tolerance.
    wand_options_ = new QWidget(options);
    auto* wand = new QHBoxLayout(wand_options_);
    wand->setContentsMargins(0, 0, 0, 0);
    wand->setSpacing(6);
    wand->addWidget(new QLabel(QStringLiteral("Tolerance"), wand_options_));
    wand_tolerance_ = new ScrubSpinBox(wand_options_);
    wand_tolerance_->setRange(0.0, 255.0);
    wand_tolerance_->setDecimals(0);
    wand_tolerance_->setValue(32.0);
    wand_tolerance_->setMaximumWidth(80);
    connect(wand_tolerance_, &QDoubleSpinBox::valueChanged, this, [this](double) { save_tool_defaults(); });
    wand->addWidget(wand_tolerance_);
    options->addWidget(wand_options_);

    // Blur tool: its mode.
    blur_options_ = new QWidget(options);
    auto* blur = new QHBoxLayout(blur_options_);
    blur->setContentsMargins(0, 0, 0, 0);
    blur->setSpacing(6);
    blur->addWidget(new QLabel(QStringLiteral("Mode"), blur_options_));
    auto* blur_mode = new QComboBox(blur_options_);
    blur_mode->addItems({QStringLiteral("Blur"), QStringLiteral("Liquify"), QStringLiteral("Smudge")});
    connect(blur_mode, &QComboBox::currentIndexChanged, this, [this](int index) { blur_mode_ = index; });
    blur->addWidget(blur_mode);
    options->addWidget(blur_options_);

    // The default tool (Pan) has no options; a tool shows its group when picked.
    brush_options_->setVisible(false);
    shape_options_->setVisible(false);
    wand_options_->setVisible(false);
    blur_options_->setVisible(false);

    color_button_->setStyleSheet(QStringLiteral("background-color: %1").arg(brush_color_.name()));
    on_brush_changed();
}

}  // namespace compositor::appwin
