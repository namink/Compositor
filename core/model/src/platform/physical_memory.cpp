#include "platform.hpp"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace compositor::model::platform {

std::uint64_t physical_memory_bytes() {
#if defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status) != 0) {
        return static_cast<std::uint64_t>(status.ullTotalPhys);
    }
    return 8ULL * 1024 * 1024 * 1024;
#elif defined(__APPLE__)
    std::uint64_t value = 0;
    std::size_t size = sizeof(value);
    int name[2] = {CTL_HW, HW_MEMSIZE};
    if (sysctl(name, 2, &value, &size, nullptr, 0) == 0 && value > 0) {
        return value;
    }
    return 8ULL * 1024 * 1024 * 1024;
#elif defined(__linux__)
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
        return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(page_size);
    }
    return 8ULL * 1024 * 1024 * 1024;
#else
    return 8ULL * 1024 * 1024 * 1024;
#endif
}

}  // namespace compositor::model::platform
