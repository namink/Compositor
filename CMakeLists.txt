cmake_minimum_required(VERSION 3.24)

project(CompositorCross LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(COMP_BUILD_TESTS "Build the cross-platform tests" ON)
option(COMP_BUILD_APP "Build the Qt6 desktop client (needs Qt6 on CMAKE_PREFIX_PATH)" OFF)

# nlohmann/json is vendored as a single header so the model builds with no package manager.
add_library(compositor_third_party INTERFACE)
target_include_directories(compositor_third_party INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

add_subdirectory(core/model)
add_subdirectory(core/pixel)
add_subdirectory(core/render)
add_subdirectory(core/io)

if(COMP_BUILD_APP)
    add_subdirectory(app/win)
endif()

if(COMP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
