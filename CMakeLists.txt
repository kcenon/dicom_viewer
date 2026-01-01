cmake_minimum_required(VERSION 3.20)

project(dicom_viewer
    VERSION 0.3.0
    DESCRIPTION "High-performance DICOM Viewer with 3D Volume Rendering"
    LANGUAGES CXX
)

# C++23 Standard (required for std::expected)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_DOCS "Build documentation" OFF)

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
    OpenGL
    OpenGLWidgets
    Test
)

find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    CommonExecutionModel
    FiltersCore
    FiltersGeneral
    FiltersSources
    GUISupportQt
    ImagingCore
    InteractionStyle
    InteractionWidgets
    IOImage
    RenderingCore
    RenderingOpenGL2
    RenderingVolumeOpenGL2
)

find_package(ITK REQUIRED COMPONENTS
    ITKCommon
    ITKIOImageBase
    ITKIOGDCM
    ITKImageFilterBase
    ITKImageFunction
    ITKSmoothing
    ITKThresholding
    ITKRegionGrowing
    ITKMathematicalMorphology
    ITKLabelMap
    ITKStatistics
    ITKVtkGlue
)

# DCMTK for DICOM network operations
find_package(DCMTK REQUIRED)

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${VTK_INCLUDE_DIRS}
    ${ITK_INCLUDE_DIRS}
)

# Qt automatic processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Core library
add_library(dicom_viewer_core STATIC
    src/core/dicom/dicom_loader.cpp
    src/core/dicom/series_builder.cpp
    src/core/dicom/transfer_syntax_decoder.cpp
    src/core/image/image_converter.cpp
    src/core/image/hounsfield_converter.cpp
    src/core/data/patient_data.cpp
)

target_include_directories(dicom_viewer_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include/core
)

target_link_libraries(dicom_viewer_core PUBLIC
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
)

# Service libraries
add_library(image_service STATIC
    src/services/image/image_service.cpp
    src/services/image/preprocessing_service.cpp
)

target_link_libraries(image_service PUBLIC
    dicom_viewer_core
    ${ITK_LIBRARIES}
)

add_library(render_service STATIC
    src/services/render/volume_renderer.cpp
    src/services/render/surface_renderer.cpp
    src/services/render/mpr_renderer.cpp
    src/services/render/transfer_function.cpp
)

target_link_libraries(render_service PUBLIC
    dicom_viewer_core
    ${VTK_LIBRARIES}
)

add_library(measurement_service STATIC
    src/services/measurement/distance_tool.cpp
    src/services/measurement/area_tool.cpp
    src/services/measurement/statistics.cpp
)

target_link_libraries(measurement_service PUBLIC
    dicom_viewer_core
    ${VTK_LIBRARIES}
    ${ITK_LIBRARIES}
)

add_library(pacs_service STATIC
    src/services/pacs/dicom_echo_scu.cpp
    src/services/pacs/dicom_find_scu.cpp
    src/services/pacs/dicom_move_scu.cpp
    src/services/pacs/pacs_config_manager.cpp
    include/services/pacs_config_manager.hpp
)

target_include_directories(pacs_service PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${DCMTK_INCLUDE_DIRS}
)

target_link_directories(pacs_service PUBLIC
    ${DCMTK_LIBRARY_DIRS}
    /opt/homebrew/lib
)

target_link_libraries(pacs_service PUBLIC
    ${DCMTK_LIBRARIES}
    spdlog::spdlog
    fmt::fmt
    Qt6::Core
)

# Controller library
add_library(dicom_viewer_controller STATIC
    src/controller/viewer_controller.cpp
    src/controller/loading_controller.cpp
    src/controller/rendering_controller.cpp
    src/controller/tool_controller.cpp
)

target_link_libraries(dicom_viewer_controller PUBLIC
    image_service
    render_service
    measurement_service
    pacs_service
)

# UI library
add_library(dicom_viewer_ui STATIC
    src/ui/main_window.cpp
    src/ui/widgets/viewport_widget.cpp
    src/ui/widgets/mpr_widget.cpp
    src/ui/panels/patient_browser.cpp
    src/ui/panels/tools_panel.cpp
    src/ui/dialogs/settings_dialog.cpp
    src/ui/dialogs/pacs_config_dialog.cpp
)

target_link_libraries(dicom_viewer_ui PUBLIC
    dicom_viewer_controller
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    VTK::GUISupportQt
)

# Main application
add_executable(dicom_viewer
    src/app/main.cpp
)

target_link_libraries(dicom_viewer PRIVATE
    dicom_viewer_ui
)

# VTK module autoinit
vtk_module_autoinit(
    TARGETS dicom_viewer dicom_viewer_ui render_service
    MODULES ${VTK_LIBRARIES}
)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install rules
install(TARGETS dicom_viewer
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
