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
    CommonTransforms
    FiltersCore
    FiltersGeneral
    FiltersGeometry
    FiltersSources
    GUISupportQt
    ImagingCore
    InteractionStyle
    InteractionWidgets
    IOGeometry
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
    ITKLevelSets
    ITKDistanceMap
    ITKWatersheds
    ITKConnectedComponents
)

# DCMTK for DICOM network operations (legacy, being migrated to pacs_system)
find_package(DCMTK REQUIRED)

# pacs_system for DICOM network operations (migration in progress)
# Option to enable pacs_system integration
option(USE_PACS_SYSTEM "Use pacs_system library for DICOM network operations" OFF)

if(USE_PACS_SYSTEM)
    # pacs_system local path configuration
    # Set PACS_SYSTEM_ROOT to the pacs_system source directory
    if(NOT PACS_SYSTEM_ROOT)
        # Default to sibling directory
        set(PACS_SYSTEM_ROOT "${CMAKE_SOURCE_DIR}/../pacs_system" CACHE PATH "Path to pacs_system source")
    endif()

    if(NOT PACS_SYSTEM_BUILD_DIR)
        set(PACS_SYSTEM_BUILD_DIR "${PACS_SYSTEM_ROOT}/build" CACHE PATH "Path to pacs_system build directory")
    endif()

    if(EXISTS "${PACS_SYSTEM_ROOT}" AND EXISTS "${PACS_SYSTEM_BUILD_DIR}/lib")
        message(STATUS "Found pacs_system at: ${PACS_SYSTEM_ROOT}")
        message(STATUS "pacs_system build directory: ${PACS_SYSTEM_BUILD_DIR}")

        # pacs_system include directory
        set(PACS_SYSTEM_INCLUDE_DIR "${PACS_SYSTEM_ROOT}/include")

        # pacs_system library directory
        set(PACS_SYSTEM_LIB_DIR "${PACS_SYSTEM_BUILD_DIR}/lib")

        # Find required pacs_system libraries
        find_library(PACS_CORE_LIB pacs_core HINTS ${PACS_SYSTEM_LIB_DIR})
        find_library(PACS_NETWORK_LIB pacs_network HINTS ${PACS_SYSTEM_LIB_DIR})
        find_library(PACS_SERVICES_LIB pacs_services HINTS ${PACS_SYSTEM_LIB_DIR})
        find_library(PACS_ENCODING_LIB pacs_encoding HINTS ${PACS_SYSTEM_LIB_DIR})
        find_library(PACS_INTEGRATION_LIB pacs_integration HINTS ${PACS_SYSTEM_LIB_DIR})

        # Find dependency libraries from pacs_system
        find_library(CONTAINER_SYSTEM_LIB container_system HINTS ${PACS_SYSTEM_LIB_DIR})
        find_library(NETWORK_SYSTEM_LIB NetworkSystem HINTS ${PACS_SYSTEM_LIB_DIR})
        find_library(LOGGER_SYSTEM_LIB LoggerSystem HINTS ${PACS_SYSTEM_LIB_DIR})

        if(PACS_CORE_LIB AND PACS_NETWORK_LIB AND PACS_SERVICES_LIB)
            set(PACS_SYSTEM_FOUND TRUE)
            message(STATUS "pacs_system libraries found:")
            message(STATUS "  - pacs_core: ${PACS_CORE_LIB}")
            message(STATUS "  - pacs_network: ${PACS_NETWORK_LIB}")
            message(STATUS "  - pacs_services: ${PACS_SERVICES_LIB}")

            # Create imported target for pacs_system
            add_library(pacs::core STATIC IMPORTED)
            set_target_properties(pacs::core PROPERTIES
                IMPORTED_LOCATION "${PACS_CORE_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${PACS_SYSTEM_INCLUDE_DIR}"
            )

            add_library(pacs::network STATIC IMPORTED)
            set_target_properties(pacs::network PROPERTIES
                IMPORTED_LOCATION "${PACS_NETWORK_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${PACS_SYSTEM_INCLUDE_DIR}"
            )

            add_library(pacs::services STATIC IMPORTED)
            set_target_properties(pacs::services PROPERTIES
                IMPORTED_LOCATION "${PACS_SERVICES_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${PACS_SYSTEM_INCLUDE_DIR}"
            )

            if(PACS_ENCODING_LIB)
                add_library(pacs::encoding STATIC IMPORTED)
                set_target_properties(pacs::encoding PROPERTIES
                    IMPORTED_LOCATION "${PACS_ENCODING_LIB}"
                    INTERFACE_INCLUDE_DIRECTORIES "${PACS_SYSTEM_INCLUDE_DIR}"
                )
            endif()

            if(PACS_INTEGRATION_LIB)
                add_library(pacs::integration STATIC IMPORTED)
                set_target_properties(pacs::integration PROPERTIES
                    IMPORTED_LOCATION "${PACS_INTEGRATION_LIB}"
                    INTERFACE_INCLUDE_DIRECTORIES "${PACS_SYSTEM_INCLUDE_DIR}"
                )
            endif()

            # Create wrapper targets for dependencies
            if(CONTAINER_SYSTEM_LIB)
                add_library(pacs::container_system STATIC IMPORTED)
                set_target_properties(pacs::container_system PROPERTIES
                    IMPORTED_LOCATION "${CONTAINER_SYSTEM_LIB}"
                )
            endif()

            if(NETWORK_SYSTEM_LIB)
                add_library(pacs::network_system STATIC IMPORTED)
                set_target_properties(pacs::network_system PROPERTIES
                    IMPORTED_LOCATION "${NETWORK_SYSTEM_LIB}"
                )
            endif()

            if(LOGGER_SYSTEM_LIB)
                add_library(pacs::logger_system STATIC IMPORTED)
                set_target_properties(pacs::logger_system PROPERTIES
                    IMPORTED_LOCATION "${LOGGER_SYSTEM_LIB}"
                )
            endif()

            add_compile_definitions(DICOM_VIEWER_USE_PACS_SYSTEM)
        else()
            set(PACS_SYSTEM_FOUND FALSE)
            message(WARNING "pacs_system libraries not found. Falling back to DCMTK only.")
        endif()
    else()
        set(PACS_SYSTEM_FOUND FALSE)
        message(WARNING "pacs_system not found at ${PACS_SYSTEM_ROOT}. Falling back to DCMTK only.")
    endif()
endif()

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

# Coordinate transformation service (unified MPR coordinate system)
add_library(coordinate_service STATIC
    src/services/coordinate/mpr_coordinate_transformer.cpp
)

target_link_libraries(coordinate_service PUBLIC
    dicom_viewer_core
    ${VTK_LIBRARIES}
)

add_library(render_service STATIC
    src/services/render/volume_renderer.cpp
    src/services/render/surface_renderer.cpp
    src/services/render/mpr_renderer.cpp
    src/services/render/transfer_function.cpp
    src/services/render/oblique_reslice_renderer.cpp
)

target_link_libraries(render_service PUBLIC
    dicom_viewer_core
    coordinate_service
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
    src/services/measurement/shape_analyzer.cpp
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
    src/services/preprocessing/histogram_equalizer.cpp
)

target_link_libraries(preprocessing_service PUBLIC
    dicom_viewer_core
    ${ITK_LIBRARIES}
)

add_library(segmentation_service STATIC
    src/services/segmentation/threshold_segmenter.cpp
    src/services/segmentation/region_growing_segmenter.cpp
    src/services/segmentation/level_set_segmenter.cpp
    src/services/segmentation/slice_interpolator.cpp
    src/services/segmentation/watershed_segmenter.cpp
    src/services/segmentation/manual_segmentation_controller.cpp
    src/services/segmentation/label_manager.cpp
    src/services/segmentation/morphological_processor.cpp
    src/services/segmentation/label_map_overlay.cpp
    src/services/segmentation/mpr_segmentation_renderer.cpp
)

target_link_libraries(segmentation_service PUBLIC
    dicom_viewer_core
    coordinate_service
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

# Add pacs_system include directories if available
if(USE_PACS_SYSTEM AND PACS_SYSTEM_FOUND)
    target_include_directories(pacs_service PUBLIC
        ${PACS_SYSTEM_INCLUDE_DIR}
    )
endif()

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

# Link pacs_system libraries if available (for gradual migration)
if(USE_PACS_SYSTEM AND PACS_SYSTEM_FOUND)
    target_link_libraries(pacs_service PUBLIC
        pacs::core
        pacs::network
        pacs::services
    )

    if(TARGET pacs::encoding)
        target_link_libraries(pacs_service PUBLIC pacs::encoding)
    endif()

    if(TARGET pacs::integration)
        target_link_libraries(pacs_service PUBLIC pacs::integration)
    endif()

    # Link pacs_system dependencies
    if(TARGET pacs::container_system)
        target_link_libraries(pacs_service PUBLIC pacs::container_system)
    endif()

    if(TARGET pacs::network_system)
        target_link_libraries(pacs_service PUBLIC pacs::network_system)
    endif()

    if(TARGET pacs::logger_system)
        target_link_libraries(pacs_service PUBLIC pacs::logger_system)
    endif()

    message(STATUS "pacs_service: pacs_system integration enabled")
endif()

add_library(export_service STATIC
    src/services/export/report_generator.cpp
    src/services/export/data_exporter.cpp
    src/services/export/measurement_serializer.cpp
    src/services/export/mesh_exporter.cpp
    src/services/export/dicom_sr_writer.cpp
)

target_include_directories(export_service PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${DCMTK_INCLUDE_DIRS}
)

# Add pacs_system include directories if available
if(USE_PACS_SYSTEM AND PACS_SYSTEM_FOUND)
    target_include_directories(export_service PUBLIC
        ${PACS_SYSTEM_INCLUDE_DIR}
    )
endif()

target_link_directories(export_service PUBLIC
    ${DCMTK_LIBRARY_DIRS}
    /opt/homebrew/lib
)

target_link_libraries(export_service PUBLIC
    dicom_viewer_core
    measurement_service
    segmentation_service
    pacs_service
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::PrintSupport
    nlohmann_json::nlohmann_json
    ${VTK_LIBRARIES}
    ${DCMTK_LIBRARIES}
)

# Note: pacs_system libraries are linked through pacs_service dependency

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
    src/ui/widgets/dr_viewer.cpp
    src/ui/panels/patient_browser.cpp
    src/ui/panels/tools_panel.cpp
    src/ui/panels/statistics_panel.cpp
    src/ui/panels/segmentation_panel.cpp
    src/ui/dialogs/settings_dialog.cpp
    src/ui/dialogs/pacs_config_dialog.cpp
    # Headers with Q_OBJECT for AUTOMOC
    include/ui/main_window.hpp
    include/ui/viewport_widget.hpp
    include/ui/mpr_view_widget.hpp
    include/ui/dr_viewer.hpp
    include/ui/panels/patient_browser.hpp
    include/ui/panels/tools_panel.hpp
    include/ui/panels/statistics_panel.hpp
    include/ui/panels/segmentation_panel.hpp
    include/ui/dialogs/pacs_config_dialog.hpp
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

# Build configuration summary
message(STATUS "")
message(STATUS "=== dicom_viewer Build Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "DICOM Network Libraries:")
message(STATUS "  DCMTK: Found (legacy)")
if(USE_PACS_SYSTEM)
    if(PACS_SYSTEM_FOUND)
        message(STATUS "  pacs_system: Found and enabled")
        message(STATUS "    Root: ${PACS_SYSTEM_ROOT}")
        message(STATUS "    Build: ${PACS_SYSTEM_BUILD_DIR}")
    else()
        message(STATUS "  pacs_system: NOT FOUND (USE_PACS_SYSTEM=ON but library not available)")
    endif()
else()
    message(STATUS "  pacs_system: Disabled (USE_PACS_SYSTEM=OFF)")
endif()
message(STATUS "")
