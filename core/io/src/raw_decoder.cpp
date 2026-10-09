#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "compositor/io/image_codec.hpp"

#ifdef COMPOSITOR_HAVE_LIBRAW
#include <libraw/libraw.h>
#endif

// Camera-raw decoding through LibRaw: demosaic, camera white balance and an sRGB output as a
// premultiplied surface (opaque). Compiled only when LibRaw is available; otherwise the calls report
// that this build has no raw support.

namespace compositor::io {
namespace {

const char* const kRawExtensions[] = {"raw", "cr2", "cr3", "crw", "nef", "nrw", "arw", "srf", "sr2", "dng",
                                      "orf", "rw2", "raf", "pef", "ptx", "srw", "x3f", "3fr", "fff", "iiq",
                                      "mos", "mrw", "erf", "kdc", "dcr", "mef", "rwl", "tif", "tiff"};

}  // namespace

bool raw_supported() {
#ifdef COMPOSITOR_HAVE_LIBRAW
    return true;
#else
    return false;
#endif
}

bool is_raw_extension(const std::string& extension) {
    std::string lowered = extension;
    if (!lowered.empty() && lowered.front() == '.') {
        lowered.erase(lowered.begin());
    }
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const char* candidate : kRawExtensions) {
        if (lowered == candidate) {
            return true;
        }
    }
    return false;
}

#ifdef COMPOSITOR_HAVE_LIBRAW
namespace {

[[nodiscard]] render::RgbaSurface to_surface(const libraw_processed_image_t* image) {
    if (image == nullptr || image->type != LIBRAW_IMAGE_BITMAP || image->colors < 3) {
        throw ImageCodecError("The raw file did not decode to a bitmap.");
    }
    const int width = image->width;
    const int height = image->height;
    render::RgbaSurface surface(width, height);
    const int channels = image->colors;
    if (image->bits == 8) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t src = (static_cast<std::size_t>(y) * width + x) * static_cast<std::size_t>(channels);
                surface.set(x, y, image->data[src], image->data[src + 1], image->data[src + 2], 255);
            }
        }
    } else if (image->bits == 16) {
        const auto* data = reinterpret_cast<const std::uint16_t*>(image->data);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t src = (static_cast<std::size_t>(y) * width + x) * static_cast<std::size_t>(channels);
                surface.set(x, y, static_cast<std::uint8_t>(data[src] >> 8),
                            static_cast<std::uint8_t>(data[src + 1] >> 8),
                            static_cast<std::uint8_t>(data[src + 2] >> 8), 255);
            }
        }
    } else {
        throw ImageCodecError("The raw file decoded to an unsupported bit depth.");
    }
    return surface;
}

}  // namespace

render::RgbaSurface decode_raw(std::span<const std::uint8_t> bytes) {
    libraw_data_t* raw = libraw_init(0);
    if (raw == nullptr) {
        throw ImageCodecError("Could not start the raw decoder.");
    }
    const auto cleanup = [&raw] {
        if (raw != nullptr) {
            libraw_close(raw);
            raw = nullptr;
        }
    };
    try {
        if (libraw_open_buffer(raw, bytes.data(), bytes.size()) != LIBRAW_SUCCESS) {
            throw ImageCodecError("That file is not a camera-raw image.");
        }
        if (libraw_unpack(raw) != LIBRAW_SUCCESS) {
            throw ImageCodecError("The raw file could not be unpacked.");
        }
        raw->params.output_bps = 8;
        raw->params.output_color = 1;  // sRGB
        raw->params.use_camera_wb = 1;
        raw->params.no_auto_bright = 0;
        if (libraw_dcraw_process(raw) != LIBRAW_SUCCESS) {
            throw ImageCodecError("The raw file could not be processed.");
        }
        int error = 0;
        libraw_processed_image_t* image = libraw_dcraw_make_mem_image(raw, &error);
        if (image == nullptr) {
            throw ImageCodecError("The raw file could not be rendered.");
        }
        render::RgbaSurface surface = to_surface(image);
        libraw_dcraw_clear_mem(image);
        cleanup();
        return surface;
    } catch (...) {
        cleanup();
        throw;
    }
}

render::RgbaSurface decode_raw_file(const std::string& path) {
    libraw_data_t* raw = libraw_init(0);
    if (raw == nullptr) {
        throw ImageCodecError("Could not start the raw decoder.");
    }
    const auto cleanup = [&raw] {
        if (raw != nullptr) {
            libraw_close(raw);
            raw = nullptr;
        }
    };
    try {
        if (libraw_open_file(raw, path.c_str()) != LIBRAW_SUCCESS) {
            throw ImageCodecError("That file is not a camera-raw image.");
        }
        if (libraw_unpack(raw) != LIBRAW_SUCCESS) {
            throw ImageCodecError("The raw file could not be unpacked.");
        }
        raw->params.output_bps = 8;
        raw->params.output_color = 1;
        raw->params.use_camera_wb = 1;
        raw->params.no_auto_bright = 0;
        if (libraw_dcraw_process(raw) != LIBRAW_SUCCESS) {
            throw ImageCodecError("The raw file could not be processed.");
        }
        int error = 0;
        libraw_processed_image_t* image = libraw_dcraw_make_mem_image(raw, &error);
        if (image == nullptr) {
            throw ImageCodecError("The raw file could not be rendered.");
        }
        render::RgbaSurface surface = to_surface(image);
        libraw_dcraw_clear_mem(image);
        cleanup();
        return surface;
    } catch (...) {
        cleanup();
        throw;
    }
}

#else  // !COMPOSITOR_HAVE_LIBRAW

render::RgbaSurface decode_raw(std::span<const std::uint8_t>) {
    throw ImageCodecError("This build has no camera-raw support (LibRaw was not found).");
}

render::RgbaSurface decode_raw_file(const std::string&) {
    throw ImageCodecError("This build has no camera-raw support (LibRaw was not found).");
}

#endif

}  // namespace compositor::io
