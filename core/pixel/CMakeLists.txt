# Portable C pixel kernels, copied verbatim from the macOS app's `Rendering/` directory. They operate
# on premultiplied 8-bit RGBA (sRGB) and are the exact math the macOS app uses, so the same inputs
# produce the same bytes here. Kept as C, exempt from the size lint (see .sizelint-allowlist).
#
# Only the kernels needed so far are here; more are copied as modules land.

add_library(compositor_pixel STATIC
    AdjustPixels.c
    LensPixels.c
    LevelsPixels.c
    NoisePixels.c
    WandPixels.c
    HealPixels.c
    ContentFill.c
    DitherPixels.c
    ScanlinesPixels.c
)

target_include_directories(compositor_pixel
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(compositor_pixel PRIVATE /W3)
    # Some kernels use M_PI, which MSVC only defines with this.
    target_compile_definitions(compositor_pixel PRIVATE _USE_MATH_DEFINES)
endif()
