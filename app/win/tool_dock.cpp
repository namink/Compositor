#include "tool_dock.hpp"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace compositor::appwin {

ToolDock::ToolDock(const QString& title, QWidget* parent) : QDockWidget(title, parent) {
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setMinimumWidth(400);
}

void ToolDock::show_content(const QString& title, QWidget* content, std::function<void()> on_apply,
                            std::function<void()> on_cancel) {
    on_apply_ = std::move(on_apply);
    on_cancel_ = std::move(on_cancel);

    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->addWidget(content, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, panel);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Apply"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (on_apply_) {
            on_apply_();
        }
        hide();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, [this] {
        if (on_cancel_) {
            on_cancel_();
        }
        hide();
    });
    layout->addWidget(buttons);

    setWindowTitle(title);
    setWidget(panel);  // takes ownership of the previous panel
    show();
    raise();
}

}  // namespace compositor::appwin
