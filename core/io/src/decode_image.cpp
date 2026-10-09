#include "compositor/io/image_codec.hpp"

namespace compositor::io {

compositor::render::RgbaSurface decode_image(std::span<const std::uint8_t> bytes) {
    if (looks_like_png(bytes)) {
        return decode_png(bytes);
    }
    if (looks_like_jpeg(bytes)) {
        return decode_jpeg(bytes);
    }
    throw ImageCodecError("Unsupported image format.");
}

}  // namespace compositor::io
