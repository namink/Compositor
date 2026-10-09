# The `.comp` project format: model, strict validation, package read/write.
#
# Kept free of Qt, Skia and image codecs so it can be unit-tested in isolation and reused by any
# front end. PNG structure is read directly from the file bytes (see png_header.cpp) rather than
# through a codec, so the model has no image dependency at all.

add_library(compositor_model STATIC
    src/uuid.cpp
    src/errors.cpp
    src/limits.cpp
    src/layer_transform.cpp
    src/blend_mode.cpp
    src/canvas_guide.cpp
    src/manifest_json.cpp
    src/validation.cpp
    src/validation_layers.cpp
    src/hierarchy.cpp
    src/live_mask.cpp
    src/png_header.cpp
    src/project_store.cpp
    src/platform/atomic_replace.cpp
    src/platform/physical_memory.cpp
)

target_include_directories(compositor_model
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(compositor_model PUBLIC compositor_third_party)

if(MSVC)
    target_compile_options(compositor_model PRIVATE /W4)
else()
    target_compile_options(compositor_model PRIVATE -Wall -Wextra)
endif()
