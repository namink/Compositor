#include <png.h>

#include <cstring>

#include "codec_support.hpp"
#include "compositor/io/image_codec.hpp"

namespace compositor::io {

std::vector<std::uint8_t> encode_png(const compositor::render::RgbaSurface& surface) {
    if (surface.empty()) {
        throw ImageCodecError("Cannot encode an empty image.");
    }
    png_image image;
    std::memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    image.width = static_cast<png_uint_32>(surface.width());
    image.height = static_cast<png_uint_32>(surface.height());
    image.format = PNG_FORMAT_RGBA;

    const std::vector<std::uint8_t> straight = detail::to_straight_rgba(surface);
    png_alloc_size_t size = 0;
    if (png_image_write_to_memory(&image, nullptr, &size, 0, straight.data(), 0, nullptr) == 0) {
        throw ImageCodecError(std::string("Could not size PNG output: ") + image.message);
    }
    std::vector<std::uint8_t> output(size);
    if (png_image_write_to_memory(&image, output.data(), &size, 0, straight.data(), 0, nullptr) == 0) {
        throw ImageCodecError(std::string("Could not encode PNG: ") + image.message);
    }
    output.resize(size);
    return output;
}

}  // namespace compositor::io
