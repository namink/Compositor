# The Windows desktop client (Qt6 Widgets). Enabled with -DCOMP_BUILD_APP=ON and a Qt6 on
# CMAKE_PREFIX_PATH, so the core still builds and tests on machines without Qt.
#
# Skia is not used yet: the composited RgbaSurface is shown as a QImage. A GPU canvas arrives later.

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
qt_standard_project_setup()

add_executable(compositor_win WIN32
    main.cpp
    main_window.cpp
    main_window_layers_dock.cpp
    main_window_options.cpp
    main_window_canvas.cpp
    main_window_edit.cpp
    main_window_menus.cpp
    main_window_tabs.cpp
    main_window_io.cpp
    main_window_drop.cpp
    main_window_watch.cpp
    main_window_layers.cpp
    main_window_layer_ops.cpp
    main_window_text_edit.cpp
    main_window_layers_panel.cpp
    main_window_history.cpp
    main_window_transform.cpp
    main_window_palette.cpp
    main_window_navigator.cpp
    main_window_defaults.cpp
    main_window_shortcuts.cpp
    tool_defaults.cpp
    tool_defaults.hpp
    shortcut_dialog.cpp
    shortcut_dialog.hpp
    tool_icons.cpp
    tool_icons.hpp
    scrub_widgets.cpp
    scrub_widgets.hpp
    color_swatch.cpp
    color_swatch.hpp
    main_window.hpp
    document_session_edit.cpp
    document_session_select.cpp
    document_session_refine.cpp
    document_session_io.cpp
    document_session_export.cpp
    document_session_paint.cpp
    document_session_warp.cpp
    adjustment_dialog.cpp
    adjustment_dialog.hpp
    text_dialog.cpp
    text_dialog.hpp
    curves_dialog.cpp
    curves_dialog.hpp
    color_range_dialog.cpp
    color_range_dialog.hpp
    levels_dialog.cpp
    levels_dialog.hpp
    layer_effects_dialog.cpp
    layer_effects_dialog.hpp
    hsv_dialog.cpp
    hsv_dialog.hpp
    camera_raw_color_dialog.cpp
    camera_raw_color_dialog.hpp
    tool_dock.cpp
    tool_dock.hpp
    shape_edit_dialog.cpp
    shape_edit_dialog.hpp
    text_runs_dialog.cpp
    text_runs_dialog.hpp
    document_session_text.cpp
    document_session_shape.cpp
    document_session_content_fill.cpp
    document_session_adjust.cpp
    document_session_masks.cpp
    document_session_canvas.cpp
    document_session_layers.cpp
    document_session_merge.cpp
    document_session_clipboard.cpp
    document_session_pixels.cpp
    canvas_view.cpp
    canvas_view_modes.cpp
    canvas_view_input.cpp
    canvas_view_overlay.cpp
    canvas_view.hpp
    document_session.cpp
    document_session_history.cpp
    document_session.hpp
    app.qrc
)

if(WIN32)
    target_sources(compositor_win PRIVATE app.rc)
endif()

target_include_directories(compositor_win PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(compositor_win
    PRIVATE
        Qt6::Widgets
        Qt6::OpenGLWidgets
        compositor_io
        compositor_render
        compositor_model
)

if(MSVC)
    target_compile_options(compositor_win PRIVATE /W4)
endif()
