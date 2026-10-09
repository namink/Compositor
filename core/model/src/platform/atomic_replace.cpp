#include <system_error>

#include "platform.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

#include "compositor/model/errors.hpp"

namespace fs = std::filesystem;

namespace compositor::model::platform {
namespace {

[[noreturn]] void fail(const std::string& what) {
    throw ProjectError(ProjectErrorCode::encode, "Could not replace the project package: " + what);
}

}  // namespace

void atomic_replace_file(const fs::path& source, const fs::path& destination) {
#if defined(_WIN32)
    if (MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        fail("MoveFileEx failed");
    }
#else
    std::error_code ec;
    fs::rename(source, destination, ec);
    if (ec) {
        fail(ec.message());
    }
#endif
}

void atomic_replace_directory(const fs::path& source, const fs::path& destination) {
    std::error_code ec;
    if (!fs::exists(destination, ec)) {
        fs::rename(source, destination, ec);
        if (ec) {
            fail(ec.message());
        }
        return;
    }

    // A non-empty directory cannot be replaced in one step, so move the old one aside first and
    // delete it only once the new one is in place. If the swap fails, put the original back.
    const fs::path backup = fs::path(destination.string() + ".bak");
    fs::remove_all(backup, ec);
    fs::rename(destination, backup, ec);
    if (ec) {
        fail("could not set the previous package aside: " + ec.message());
    }
    fs::rename(source, destination, ec);
    if (ec) {
        fs::rename(backup, destination, ec);  // best effort rollback
        fail("could not move the new package into place: " + ec.message());
    }
    fs::remove_all(backup, ec);
}

}  // namespace compositor::model::platform
