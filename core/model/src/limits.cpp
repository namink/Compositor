#include "compositor/model/limits.hpp"

#include "platform/platform.hpp"

namespace compositor::model {

std::int64_t DocumentLimits::document_pixel_budget() {
    constexpr std::int64_t kCeiling = 800'000'000;
    constexpr std::int64_t kFloor = kMaxSurfacePixels;
    const std::int64_t from_memory = static_cast<std::int64_t>(platform::physical_memory_bytes() / 16U);
    const std::int64_t clipped = from_memory < kFloor ? kFloor : from_memory;
    return clipped > kCeiling ? kCeiling : clipped;
}

}  // namespace compositor::model
