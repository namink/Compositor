#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "compositor/render/guided_matte.hpp"
#include "document_session.hpp"

// Refine Edge, split out so document_session_select.cpp stays within the soft size limit.

namespace compositor::appwin {
bool DocumentSession::refine_selection_edges(int radius, QString& error) {
    if (!snapshot_) {
        error = QStringLiteral("No project is open.");
        return false;
    }
    if (!selection_ || selection_->empty()) {
        error = QStringLiteral("Select an area first.");
        return false;
    }
    const int width = selection_->width;
    const int height = selection_->height;
    if (flat_.width() != width || flat_.height() != height) {
        error = QStringLiteral("The selection does not match the canvas.");
        return false;
    }
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<float> mask(count, 0.0F);
    std::vector<float> guide(count, 0.0F);
    for (std::size_t i = 0; i < count; ++i) {
        mask[i] = static_cast<float>(selection_->coverage[i]) / 255.0F;
        const std::uint8_t* pixel = flat_.data() + i * 4U;
        const float alpha = static_cast<float>(pixel[3]);
        if (alpha > 0.0F) {
            const float luminance = (0.2126F * pixel[0] + 0.7152F * pixel[1] + 0.0722F * pixel[2]) / alpha / 255.0F;
            guide[i] = std::clamp(luminance, 0.0F, 1.0F);
        }
    }
    const std::vector<float> refined = render::guided_filter(mask, guide, width, height, std::max(1, radius), 1e-4F);
    int min_x = width;
    int min_y = height;
    int max_x = -1;
    int max_y = -1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
            const auto coverage =
                static_cast<std::uint8_t>(std::lround(std::clamp(refined[index], 0.0F, 1.0F) * 255.0F));
            selection_->coverage[index] = coverage;
            if (coverage != 0) {
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        }
    }
    selection_->min_x = min_x;
    selection_->min_y = min_y;
    selection_->max_x = max_x;
    selection_->max_y = max_y;
    return true;
}

}  // namespace compositor::appwin
