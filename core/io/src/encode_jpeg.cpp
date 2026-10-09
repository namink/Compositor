// clang-format off: <stdio.h> must precede <jpeglib.h>, which uses FILE.
#include <stdio.h>
#include <jpeglib.h>
// clang-format on

#include <algorithm>
#include <csetjmp>
#include <cstdlib>
#include <vector>

#include "compositor/io/image_codec.hpp"

namespace compositor::io {
namespace {

struct ErrorManager {
    jpeg_error_mgr base;
    std::jmp_buf jump;
};

void on_error(j_common_ptr info) {
    auto* manager = reinterpret_cast<ErrorManager*>(info->err);
    std::longjmp(manager->jump, 1);
}

}  // namespace

std::vector<std::uint8_t> encode_jpeg(const compositor::render::RgbaSurface& surface, int quality) {
    if (surface.empty()) {
        throw ImageCodecError("Cannot encode an empty image.");
    }
    jpeg_compress_struct info{};
    ErrorManager errors{};
    info.err = jpeg_std_error(&errors.base);
    errors.base.error_exit = on_error;
    unsigned char* output = nullptr;
    unsigned long output_size = 0;
    if (setjmp(errors.jump) != 0) {
        jpeg_destroy_compress(&info);
        std::free(output);
        throw ImageCodecError("Could not encode JPEG.");
    }
    jpeg_create_compress(&info);
    jpeg_mem_dest(&info, &output, &output_size);
    info.image_width = static_cast<JDIMENSION>(surface.width());
    info.image_height = static_cast<JDIMENSION>(surface.height());
    info.input_components = 3;
    info.in_color_space = JCS_RGB;
    jpeg_set_defaults(&info);
    jpeg_set_quality(&info, std::clamp(quality, 1, 100), TRUE);
    jpeg_start_compress(&info, TRUE);

    const std::uint8_t* pixels = surface.data();
    std::vector<std::uint8_t> row(static_cast<std::size_t>(surface.width()) * 3U);
    while (info.next_scanline < info.image_height) {
        const int y = static_cast<int>(info.next_scanline);
        for (int x = 0; x < surface.width(); ++x) {
            const std::size_t at = (static_cast<std::size_t>(y) * surface.width() + x) * 4U;
            const int alpha = pixels[at + 3];
            // Premultiplied pixel composited on white: straight * alpha + (1 - alpha) * 255.
            row[static_cast<std::size_t>(x) * 3U] = static_cast<std::uint8_t>(pixels[at] + (255 - alpha));
            row[static_cast<std::size_t>(x) * 3U + 1U] = static_cast<std::uint8_t>(pixels[at + 1] + (255 - alpha));
            row[static_cast<std::size_t>(x) * 3U + 2U] = static_cast<std::uint8_t>(pixels[at + 2] + (255 - alpha));
        }
        JSAMPROW target = row.data();
        jpeg_write_scanlines(&info, &target, 1);
    }
    jpeg_finish_compress(&info);
    jpeg_destroy_compress(&info);
    std::vector<std::uint8_t> result(output, output + output_size);
    std::free(output);
    return result;
}

}  // namespace compositor::io
