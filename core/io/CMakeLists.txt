# Image codecs. libpng and libjpeg-turbo come from vcpkg (see vcpkg.json); the public headers do not
# expose either library, so they link privately.

find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
# Camera-raw support is optional: when LibRaw is installed (via vcpkg), raw_decoder.cpp compiles the
# real path, otherwise it reports that the build has no raw support.
find_package(libraw CONFIG QUIET)

add_library(compositor_io STATIC
    src/decode_png.cpp
    src/decode_jpeg.cpp
    src/decode_image.cpp
    src/encode_png.cpp
    src/encode_jpeg.cpp
    src/raw_decoder.cpp
    src/psd_channel.cpp
    src/psd_record.cpp
    src/psd_reader.cpp
    src/psd_writer.cpp
)

if(libraw_FOUND)
    target_link_libraries(compositor_io PRIVATE libraw::raw_r)
    target_compile_definitions(compositor_io PRIVATE COMPOSITOR_HAVE_LIBRAW)
endif()

target_include_directories(compositor_io
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(compositor_io
    PUBLIC compositor_render
    PRIVATE PNG::PNG JPEG::JPEG
)

if(MSVC)
    # 4324: jmp_buf alignment padding in the libjpeg error manager. 4611: setjmp vs C++ destructors,
    # which is inherent to libjpeg's error handling. Both are expected here.
    target_compile_options(compositor_io PRIVATE /W4 /wd4324 /wd4611)
else()
    target_compile_options(compositor_io PRIVATE -Wall -Wextra)
endif()
