// clang-format off: <stdio.h> must precede <jpeglib.h>, which uses FILE.
#include <stdio.h>
#include <jpeglib.h>
// clang-format on

#include <csetjmp>
#include <cstdlib>
#include <vector>

#include "codec_support.hpp"
#include "compositor/io/image_codec.hpp"

namespace compositor::io {
namespace {

/// libjpeg reports errors with `longjmp`; this carries the jump target and turns the fall-out into
/// an exception at the call site.
struct ErrorManager {
    jpeg_error_mgr base;
    std::jmp_buf jump;
};

void on_error(j_common_ptr info) {
    auto* manager = reinterpret_cast<ErrorManager*>(info->err);
    std::longjmp(manager->jump, 1);
}

}  // namespace

bool looks_like_jpeg(std::span<const std::uint8_t> bytes) {
    return bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF;
}

compositor::render::RgbaSurface decode_jpeg(std::span<const std::uint8_t> bytes) {
    if (!looks_like_jpeg(bytes)) {
        throw ImageCodecError("Not a JPEG file.");
    }
    jpeg_decompress_struct info{};
    ErrorManager errors{};
    info.err = jpeg_std_error(&errors.base);
    errors.base.error_exit = on_error;
    if (setjmp(errors.jump) != 0) {
        jpeg_destroy_decompress(&info);
        throw ImageCodecError("Could not decode JPEG.");
    }
    jpeg_create_decompress(&info);
    jpeg_mem_src(&info, bytes.data(), static_cast<unsigned long>(bytes.size()));
    jpeg_read_header(&info, TRUE);
    info.out_color_space = JCS_RGB;
    jpeg_start_decompress(&info);

    const int width = static_cast<int>(info.output_width);
    const int height = static_cast<int>(info.output_height);
    std::vector<std::uint8_t> row(static_cast<std::size_t>(width) * 3U);
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
    while (info.output_scanline < info.output_height) {
        JSAMPROW target = row.data();
        jpeg_read_scanlines(&info, &target, 1);
        const int y = static_cast<int>(info.output_scanline) - 1;
        for (int x = 0; x < width; ++x) {
            const std::size_t to = (static_cast<std::size_t>(y) * width + x) * 4U;
            const std::size_t from = static_cast<std::size_t>(x) * 3U;
            rgba[to] = row[from];
            rgba[to + 1] = row[from + 1];
            rgba[to + 2] = row[from + 2];
            rgba[to + 3] = 255;  // JPEG has no alpha.
        }
    }
    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);
    return detail::from_straight_rgba(rgba.data(), width, height);
}

}  // namespace compositor::io
