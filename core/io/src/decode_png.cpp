#include <png.h>

#include <cstring>

#include "codec_support.hpp"
#include "compositor/io/image_codec.hpp"

namespace compositor::io {
namespace {

constexpr std::uint8_t kPngSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

}  // namespace

bool looks_like_png(std::span<const std::uint8_t> bytes) {
    return bytes.size() >= sizeof(kPngSignature) &&
           std::memcmp(bytes.data(), kPngSignature, sizeof(kPngSignature)) == 0;
}

compositor::render::RgbaSurface decode_png(std::span<const std::uint8_t> bytes) {
    if (!looks_like_png(bytes)) {
        throw ImageCodecError("Not a PNG file.");
    }
    png_image image;
    std::memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(&image, bytes.data(), bytes.size()) == 0) {
        throw ImageCodecError(std::string("Could not read PNG: ") + image.message);
    }
    image.format = PNG_FORMAT_RGBA;  // 8-bit RGBA, expanding grayscale, palette and alpha as needed.
    std::vector<std::uint8_t> raw(PNG_IMAGE_SIZE(image));
    if (png_image_finish_read(&image, nullptr, raw.data(), 0, nullptr) == 0) {
        const std::string message = image.message;
        png_image_free(&image);
        throw ImageCodecError(std::string("Could not decode PNG: ") + message);
    }
    const int width = static_cast<int>(image.width);
    const int height = static_cast<int>(image.height);
    png_image_free(&image);
    return detail::from_straight_rgba(raw.data(), width, height);
}

}  // namespace compositor::io
