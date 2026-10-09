include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(compositor_model_tests
    model/test_manifest_json.cpp
    model/test_validation.cpp
    model/test_project_store.cpp
)

target_include_directories(compositor_model_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(compositor_model_tests PRIVATE compositor_model GTest::gtest_main)

add_test(NAME compositor_model_tests COMMAND compositor_model_tests)

add_executable(compositor_render_tests
    render/test_blend_modes.cpp
    render/test_compositor.cpp
    render/test_adjustment.cpp
    render/test_auto_levels.cpp
    render/test_blur.cpp
    render/test_effects.cpp
    render/test_paint.cpp
    render/test_content_fill.cpp
    render/test_shape.cpp
    render/test_warp.cpp
    render/test_guided_matte.cpp
    render/test_selection.cpp
)

target_include_directories(compositor_render_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(compositor_render_tests PRIVATE compositor_render compositor_model GTest::gtest_main)

add_test(NAME compositor_render_tests COMMAND compositor_render_tests)

add_executable(compositor_io_tests
    io/test_image_codec.cpp
    io/test_psd_reader.cpp
    io/test_psd_writer.cpp
    io/test_raw.cpp
)

target_include_directories(compositor_io_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(compositor_io_tests PRIVATE compositor_io GTest::gtest_main)

add_test(NAME compositor_io_tests COMMAND compositor_io_tests)

add_executable(compositor_pipeline_tests
    integration/test_comp_pipeline.cpp
)

target_include_directories(compositor_pipeline_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(compositor_pipeline_tests
    PRIVATE compositor_io compositor_render compositor_model GTest::gtest_main)

add_test(NAME compositor_pipeline_tests COMMAND compositor_pipeline_tests)
