#pragma once
#include <QColor>
#include <QWidget>
#include <functional>

namespace compositor::appwin {

/// The foreground/background color squares at the bottom of the tool rail, as the macOS app shows
/// them: the foreground square on top-left, the background behind bottom-right, and a swap arrow.
/// Left-click picks the foreground, right-click the background, the arrow swaps them.
class ColorSwatch : public QWidget {
public:
    explicit ColorSwatch(QWidget* parent = nullptr);

    void set_foreground(const QColor& color);
    void set_background(const QColor& color);

    std::function<void(const QColor&)> on_foreground;
    std::function<void(const QColor&)> on_background;
    std::function<void()> on_swap;

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QColor foreground_{0, 0, 0};
    QColor background_{255, 255, 255};
    int foreground_y() const;
    int background_y() const;
    int square_size() const;
    int swap_y() const;
};

}  // namespace compositor::appwin
