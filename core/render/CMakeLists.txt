# The compositing core: blend math, layer placement, the flattening compositor.
#
# Pure in-memory RGBA work with no image codecs, so it unit-tests directly. PNG decoding and the GPU
# path plug into it later.

add_library(compositor_render STATIC
    src/rgba_surface.cpp
    src/blend_mode_math.cpp
    src/placement.cpp
    src/render_order.cpp
    src/compositor.cpp
    src/adjustment.cpp
    src/camera_raw_color.cpp
    src/auto_levels.cpp
    src/adjustment_hsv.cpp
    src/adjustment_curves.cpp
    src/blur.cpp
    src/effects.cpp
    src/effects_helpers.cpp
    src/paint.cpp
    src/warp.cpp
    src/content_fill.cpp
    src/shape.cpp
    src/guided_matte.cpp
    src/selection.cpp
)

target_include_directories(compositor_render
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(compositor_render
    PUBLIC compositor_model
    PRIVATE compositor_pixel
)

if(MSVC)
    target_compile_options(compositor_render PRIVATE /W4)
else()
    target_compile_options(compositor_render PRIVATE -Wall -Wextra)
endif()
