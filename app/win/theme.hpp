#pragma once
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPalette>

namespace compositor::appwin {

/// A dark theme that follows the macOS app's chrome as closely as Qt allows. The macOS look is native
/// (system accent, SF Pro, materials), so this approximates it: the same dark grays, a system-blue
/// accent, 4/6 px corners, 12 pt control text and a 42 px tool header. Fonts fall back to Segoe UI
/// where SF Pro is unavailable.
inline void apply_dark_theme(QApplication& app) {
    app.setStyle(QStringLiteral("Fusion"));

    // macOS system blue (dark appearance), used for the accent.
    const QColor accent(10, 132, 255);
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(30, 30, 30));
    palette.setColor(QPalette::WindowText, QColor(224, 224, 224));
    palette.setColor(QPalette::Base, QColor(23, 23, 23));
    palette.setColor(QPalette::AlternateBase, QColor(38, 38, 38));
    palette.setColor(QPalette::ToolTipBase, QColor(38, 38, 38));
    palette.setColor(QPalette::ToolTipText, QColor(230, 230, 230));
    palette.setColor(QPalette::Text, QColor(224, 224, 224));
    palette.setColor(QPalette::Button, QColor(51, 51, 51));
    palette.setColor(QPalette::ButtonText, QColor(224, 224, 224));
    palette.setColor(QPalette::BrightText, QColor(255, 90, 90));
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
    app.setPalette(palette);

    // SF Pro is unavailable; Segoe UI is the closest Windows face for the app's 12 pt controls.
    QFont base = app.font();
    base.setFamily(QStringLiteral("Segoe UI"));
    base.setPointSizeF(9.0);
    app.setFont(base);

    // The macOS chrome: flat tool strip with accent-highlighted tools, slim dock titles, pill tab
    // strip (6 px gaps), 12 pt tool header, compact rounded inputs and menus.
    const QString sheet = QStringLiteral(R"(
        QToolBar { background: #1e1e1e; border: 0; spacing: 4px; padding: 4px 8px; }
        QToolBar#optionsBar { font-size: 12px; }
        QToolBar#optionsBar QLabel { font-size: 12px; color: #b8b8b8; }
        QToolBar#toolStrip { spacing: 3px; padding: 6px 4px; }
        QToolButton { padding: 5px 9px; border: 0; border-radius: 6px; }
        QToolButton:hover { background: #303030; }
        QToolButton:checked { background: #0a84ff; color: #ffffff; }
        QDockWidget { titlebar-close-icon: none; titlebar-normal-icon: none; }
        QDockWidget::title { background: #262626; padding: 6px 10px; font-weight: 600; }
        QTabBar { qproperty-drawBase: 0; }
        QTabBar::tab { background: #262626; padding: 5px 14px; border: 0; border-radius: 6px; margin: 3px 3px; }
        QTabBar::tab:selected { background: #0a84ff; color: #ffffff; }
        QTabBar::tab:hover:!selected { background: #333333; }
        QLineEdit, QComboBox, QAbstractSpinBox, QKeySequenceEdit {
            background: #171717; border: 1px solid #3a3a3a; border-radius: 4px; padding: 2px 6px;
        }
        QComboBox::drop-down, QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { border: 0; width: 14px; }
        QListWidget, QTreeWidget { background: #1c1c1c; border: 0; outline: 0; }
        QListWidget::item, QTreeWidget::item { padding: 4px; border-radius: 4px; }
        QListWidget::item:selected, QTreeWidget::item:selected { background: #0a84ff; color: #ffffff; }
        QSlider::groove:horizontal { height: 4px; background: #3a3a3a; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #0a84ff; border-radius: 2px; }
        QSlider::handle:horizontal { width: 12px; margin: -5px 0; border-radius: 6px; background: #d0d0d0; }
        QSlider::handle:horizontal:hover { background: #ffffff; }
        QMenu { background: #262626; border: 1px solid #3a3a3a; border-radius: 6px; padding: 4px; }
        QMenu::item { padding: 4px 20px; border-radius: 4px; }
        QMenu::item:selected { background: #0a84ff; }
        QMenu::separator { height: 1px; background: #3a3a3a; margin: 4px 6px; }
        QPushButton { background: #333333; border: 1px solid #3a3a3a; border-radius: 6px; padding: 4px 12px; }
        QPushButton:hover { background: #3d3d3d; }
        QPushButton:pressed { background: #2a2a2a; }
        QPushButton:default { background: #0a84ff; border-color: #0a84ff; color: #ffffff; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
        QScrollBar::handle:vertical { background: #4a4a4a; border-radius: 5px; min-height: 24px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
        QScrollBar::handle:horizontal { background: #4a4a4a; border-radius: 5px; min-width: 24px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QStatusBar { background: #1e1e1e; }
        QDialog { background: #1e1e1e; }
    )");
    app.setStyleSheet(sheet);
}

}  // namespace compositor::appwin
