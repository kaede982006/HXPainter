cmake_minimum_required(VERSION 3.21)

project(HXPainter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use CMAKE_CURRENT_SOURCE_DIR for logo to ensure it's found during out-of-source builds
set(HXPAINTER_LOGO "${CMAKE_CURRENT_SOURCE_DIR}/logo.png")
if(NOT EXISTS "${HXPAINTER_LOGO}")
    message(FATAL_ERROR "Required logo file is missing: ${HXPAINTER_LOGO}")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

# Avoid manifest errors during cross-compilation
if(WIN32 AND CMAKE_CROSSCOMPILING)
    set(QT_NO_WINDOWS_FINALIZATION TRUE)
endif()

set(HXPAINTER_WINDOWS_RESOURCES)
if(WIN32)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(HXPAINTER_ICON_ICO "${CMAKE_CURRENT_BINARY_DIR}/generated/HXPainter.ico")
        execute_process(
            COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_icon.py"
                    "${HXPAINTER_LOGO}"
                    "${HXPAINTER_ICON_ICO}"
            RESULT_VARIABLE HXPAINTER_ICON_RESULT
            OUTPUT_VARIABLE HXPAINTER_ICON_OUTPUT
            ERROR_VARIABLE HXPAINTER_ICON_ERROR
        )

        if(HXPAINTER_ICON_RESULT EQUAL 0 AND EXISTS "${HXPAINTER_ICON_ICO}")
            file(TO_CMAKE_PATH "${HXPAINTER_ICON_ICO}" HXPAINTER_ICON_ICO_NATIVE)
            configure_file(
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/HXPainter.rc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/generated/HXPainter.rc"
                @ONLY
            )
            list(APPEND HXPAINTER_WINDOWS_RESOURCES
                "${CMAKE_CURRENT_BINARY_DIR}/generated/HXPainter.rc"
            )
            message(STATUS "Generated Windows executable icon: ${HXPAINTER_ICON_ICO}")
        else()
            message(WARNING "Could not generate Windows .ico from logo.png. Runtime Qt icon loading will still be used. ${HXPAINTER_ICON_ERROR}")
        endif()
    else()
        message(WARNING "Python3 was not found; skipping optional Windows executable icon generation.")
    endif()
endif()

# Use CMAKE_CURRENT_SOURCE_DIR for platform-specific files to avoid path resolution errors
set(PLATFORM_SOURCES)
if(WIN32)
    list(APPEND PLATFORM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/win/WinSpecific.cpp")
elseif(UNIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/linux/LinuxSpecific.cpp")
endif()

qt_add_executable(HXPainter
    src/main.cpp
    ${PLATFORM_SOURCES}
    src/app/AppPaths.cpp
    src/app/AppPaths.h
    src/app/IconLoader.cpp
    src/app/IconLoader.h
    src/actions/ActionRegistry.cpp
    src/actions/ActionRegistry.h
    src/core/PerformanceStats.cpp
    src/core/PerformanceStats.h
    src/core/TabletInputEvent.h
    src/commands/Command.cpp
    src/commands/Command.h
    src/commands/CommandManager.cpp
    src/commands/CommandManager.h
    src/document/LayerManager.cpp
    src/document/LayerManager.h
    src/input/TabletInputMapper.cpp
    src/input/TabletInputMapper.h
    src/brush/BrushEngine.cpp
    src/brush/BrushEngine.h
    src/brush/EraserEngine.cpp
    src/brush/EraserEngine.h
    src/brush/FillEngine.cpp
    src/brush/FillEngine.h
    src/brush/TextEngine.cpp
    src/brush/TextEngine.h
    src/brush/BrushSettings.h
    src/export/ExportManager.cpp
    src/export/ExportManager.h
    src/render/CanvasDocument.cpp
    src/render/CanvasDocument.h
    src/render/OpenGLCanvasWidget.cpp
    src/render/OpenGLCanvasWidget.h
    src/serialization/ProjectSerializer.cpp
    src/serialization/ProjectSerializer.h
    src/tools/Tool.cpp
    src/tools/Tool.h
    src/tools/ToolManager.cpp
    src/tools/ToolManager.h
    src/ui/CreateOpenHub.cpp
    src/ui/CreateOpenHub.h
    src/ui/ExportDialog.cpp
    src/ui/ExportDialog.h
    src/ui/TextSettingsDialog.cpp
    src/ui/TextSettingsDialog.h
    src/ui/AppTheme.cpp
    src/ui/AppTheme.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/panels/ToolOptionsPanel.cpp
    src/ui/panels/ToolOptionsPanel.h
    resources/hxpainter.qrc
    ${HXPAINTER_WINDOWS_RESOURCES}
)

target_link_libraries(HXPainter PRIVATE
    Qt6::Widgets
    Qt6::Svg
)

target_include_directories(HXPainter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(HXPainter PRIVATE
    QT_NO_KEYWORDS
)

if(WIN32)
    set_target_properties(HXPainter PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

add_custom_command(TARGET HXPainter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${HXPAINTER_LOGO}"
            "$<TARGET_FILE_DIR:HXPainter>/logo.png"
    COMMENT "Copying logo.png next to HXPainter executable"
)

install(TARGETS HXPainter
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
    LIBRARY DESTINATION .
)

install(FILES "${HXPAINTER_LOGO}" DESTINATION .)

enable_testing()
add_test(NAME HXPainterIconSmoke
    COMMAND $<TARGET_FILE:HXPainter> --smoke-icon-test
)

add_test(NAME HXPainterMvpSmoke
    COMMAND $<TARGET_FILE:HXPainter> --mvp-smoke-test
)

add_test(NAME HXPainterThemeSmoke
    COMMAND $<TARGET_FILE:HXPainter> --theme-smoke-test
)

add_test(NAME HXPainterStartupSmoke
    COMMAND $<TARGET_FILE:HXPainter> --startup-smoke-test
)

add_test(NAME HXPainterFunctionalRegressionSmoke
    COMMAND $<TARGET_FILE:HXPainter> --functional-regression-smoke-test
)
