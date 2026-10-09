#include <vector>

#include "compositor/render/paint.hpp"

extern "C" {
#include "ContentFill.h"
}

namespace compositor::render {

bool content_fill(RgbaSurface& surface, const Selection& selection) {
    if (surface.empty() || selection.empty() || selection.width != surface.width() ||
        selection.height != surface.height()) {
        return false;
    }
    // The kernel reads a single mask byte per pixel: selected pixels are the ones to synthesize.
    std::vector<std::uint8_t> mask(selection.coverage.begin(), selection.coverage.end());
    const int result = ::content_fill(surface.data(), static_cast<std::size_t>(surface.width()) * 4U, mask.data(),
                                      static_cast<std::size_t>(surface.width()), surface.width(), surface.height());
    return result == 1;
}

}  // namespace compositor::render
