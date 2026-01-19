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

# macOS SDK configuration - avoid math.h macro conflicts with C++ standard library
if(APPLE)
    # Try to find a compatible SDK in order of preference
    # Prefer newer stable SDKs from Command Line Tools
    set(SDK_SEARCH_PATHS
        "/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk"
        "/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk"
        "/Library/Developer/CommandLineTools/SDKs/MacOSX14.5.sdk"
        "/Library/Developer/CommandLineTools/SDKs/MacOSX14.sdk"
    )

    set(FOUND_SDK_PATH "")
    foreach(SDK_PATH ${SDK_SEARCH_PATHS})
        if(EXISTS "${SDK_PATH}")
            set(FOUND_SDK_PATH "${SDK_PATH}")
            break()
        endif()
    endforeach()

    if(FOUND_SDK_PATH)
        set(CMAKE_OSX_SYSROOT "${FOUND_SDK_PATH}" CACHE PATH "macOS SDK path" FORCE)
        message(STATUS "Using Command Line Tools SDK: ${CMAKE_OSX_SYSROOT}")

        # Force the compiler to use this SDK for ALL headers including system headers
        # This prevents mixing of different SDK versions
        add_compile_options(
            -isysroot "${FOUND_SDK_PATH}"
            -I"${FOUND_SDK_PATH}/usr/include"
        )
        add_link_options(
            -isysroot "${FOUND_SDK_PATH}"
            -L"${FOUND_SDK_PATH}/usr/lib"
        )
    else()
        execute_process(
            COMMAND xcrun --sdk macosx --show-sdk-path
            OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        message(STATUS "Using Xcode SDK: ${CMAKE_OSX_SYSROOT}")
    endif()

    # Set deployment target for compatibility
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS version" FORCE)
    endif()

    add_compile_definitions(
        _USE_MATH_DEFINES
    )

    add_compile_options(
        -Wno-deprecated-declarations
    )
endif()

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
    PrintSupport
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
    ITKIONIFTI
    ITKIONRRD
    ITKImageFilterBase
    ITKImageFunction
    ITKSmoothing
    ITKThresholding
    ITKRegionGrowing
    ITKMathematicalMorphology
    ITKLabelMap
    ITKStatistics
    ITKBiasCorrection
    ITKVtkGlue
)

# DCMTK for DICOM network operations
find_package(DCMTK REQUIRED)

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)

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

# Find FFTW for ITK's FFT operations (required by N4 bias correction and other filters)
find_library(FFTW3_LIBRARY fftw3 HINTS /opt/homebrew/lib)
find_library(FFTW3_THREADS_LIBRARY fftw3_threads HINTS /opt/homebrew/lib)

# Core library
add_library(dicom_viewer_core STATIC
    src/core/logging/logging.cpp
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

target_link_directories(dicom_viewer_core PUBLIC
    /opt/homebrew/lib
)

target_link_libraries(dicom_viewer_core PUBLIC
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
    ${FFTW3_LIBRARY}
    ${FFTW3_THREADS_LIBRARY}
    spdlog::spdlog
    fmt::fmt
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
    src/services/render/mpr_coordinate_transformer.cpp
    src/services/render/transfer_function.cpp
    src/services/render/oblique_reslice_renderer.cpp
)

target_link_libraries(render_service PUBLIC
    dicom_viewer_core
    ${VTK_LIBRARIES}
)

add_library(measurement_service STATIC
    src/services/measurement/linear_measurement_tool.cpp
    src/services/measurement/area_measurement_tool.cpp
    src/services/measurement/distance_tool.cpp
    src/services/measurement/area_tool.cpp
    src/services/measurement/statistics.cpp
    src/services/measurement/roi_statistics.cpp
    src/services/measurement/volume_calculator.cpp
)

target_link_libraries(measurement_service PUBLIC
    dicom_viewer_core
    ${VTK_LIBRARIES}
    ${ITK_LIBRARIES}
)

add_library(preprocessing_service STATIC
    src/services/preprocessing/gaussian_smoother.cpp
    src/services/preprocessing/anisotropic_diffusion_filter.cpp
    src/services/preprocessing/n4_bias_corrector.cpp
    src/services/preprocessing/isotropic_resampler.cpp
)

target_link_libraries(preprocessing_service PUBLIC
    dicom_viewer_core
    ${ITK_LIBRARIES}
)

add_library(segmentation_service STATIC
    src/services/segmentation/threshold_segmenter.cpp
    src/services/segmentation/region_growing_segmenter.cpp
    src/services/segmentation/manual_segmentation_controller.cpp
    src/services/segmentation/label_manager.cpp
    src/services/segmentation/morphological_processor.cpp
    src/services/segmentation/label_map_overlay.cpp
    src/services/segmentation/mpr_coordinate_transformer.cpp
    src/services/segmentation/mpr_segmentation_renderer.cpp
)

target_link_libraries(segmentation_service PUBLIC
    dicom_viewer_core
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
    nlohmann_json::nlohmann_json
)

add_library(pacs_service STATIC
    src/services/pacs/dicom_echo_scu.cpp
    src/services/pacs/dicom_find_scu.cpp
    src/services/pacs/dicom_move_scu.cpp
    src/services/pacs/dicom_store_scp.cpp
    src/services/pacs/pacs_config_manager.cpp
    include/services/pacs_config_manager.hpp
    include/services/dicom_store_scp.hpp
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

add_library(export_service STATIC
    src/services/export/report_generator.cpp
    src/services/export/data_exporter.cpp
    src/services/export/measurement_serializer.cpp
)

target_include_directories(export_service PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(export_service PUBLIC
    measurement_service
    segmentation_service
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::PrintSupport
    nlohmann_json::nlohmann_json
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
    segmentation_service
    pacs_service
    export_service
)

# UI library
add_library(dicom_viewer_ui STATIC
    src/ui/main_window.cpp
    src/ui/widgets/viewport_widget.cpp
    src/ui/widgets/mpr_widget.cpp
    src/ui/widgets/mpr_view_widget.cpp
    src/ui/panels/patient_browser.cpp
    src/ui/panels/tools_panel.cpp
    src/ui/panels/statistics_panel.cpp
    src/ui/panels/segmentation_panel.cpp
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
