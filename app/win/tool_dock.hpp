#pragma once
#include <QDockWidget>
#include <functional>

class QWidget;

namespace compositor::appwin {

/// A right-docked tool panel (macOS's floating `NSPanel`): non-modal, with Apply/Cancel, showing the
/// live-previewing controls of a filter or tool. The window keeps one and swaps its content.
class ToolDock : public QDockWidget {
public:
    explicit ToolDock(const QString& title, QWidget* parent = nullptr);

    void show_content(const QString& title, QWidget* content, std::function<void()> on_apply,
                      std::function<void()> on_cancel);

private:
    std::function<void()> on_apply_;
    std::function<void()> on_cancel_;
};

}  // namespace compositor::appwin
