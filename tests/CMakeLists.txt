enable_testing()

# Find Google Test
find_package(GTest REQUIRED)

# Unit tests for series assembly
add_executable(series_assembly_test
    unit/series_assembly_test.cpp
)

target_link_libraries(series_assembly_test PRIVATE
    dicom_viewer_core
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(series_assembly_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Unit tests for transfer syntax decoder
add_executable(transfer_syntax_decoder_test
    unit/transfer_syntax_decoder_test.cpp
)

target_link_libraries(transfer_syntax_decoder_test PRIVATE
    dicom_viewer_core
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(transfer_syntax_decoder_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Unit tests for Hounsfield converter
add_executable(hounsfield_converter_test
    unit/hounsfield_converter_test.cpp
)

target_link_libraries(hounsfield_converter_test PRIVATE
    dicom_viewer_core
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(hounsfield_converter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Unit tests for Volume Renderer
add_executable(volume_renderer_test
    unit/volume_renderer_test.cpp
)

target_link_libraries(volume_renderer_test PRIVATE
    render_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(volume_renderer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Unit tests for TransferFunctionManager
add_executable(transfer_function_manager_test
    unit/transfer_function_manager_test.cpp
)

target_link_libraries(transfer_function_manager_test PRIVATE
    render_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(transfer_function_manager_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Unit tests for MPR Renderer
add_executable(mpr_renderer_test
    unit/mpr_renderer_test.cpp
)

target_link_libraries(mpr_renderer_test PRIVATE
    render_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(mpr_renderer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Unit tests for Surface Renderer
add_executable(surface_renderer_test
    unit/surface_renderer_test.cpp
)

target_link_libraries(surface_renderer_test PRIVATE
    render_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(surface_renderer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Unit tests for DICOM Echo SCU
add_executable(dicom_echo_scu_test
    unit/dicom_echo_scu_test.cpp
)

target_link_libraries(dicom_echo_scu_test PRIVATE
    pacs_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(dicom_echo_scu_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Unit tests for DICOM Find SCU
add_executable(dicom_find_scu_test
    unit/dicom_find_scu_test.cpp
)

target_link_libraries(dicom_find_scu_test PRIVATE
    pacs_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(dicom_find_scu_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# VTK module autoinit for tests
vtk_module_autoinit(
    TARGETS volume_renderer_test transfer_function_manager_test mpr_renderer_test surface_renderer_test
    MODULES ${VTK_LIBRARIES}
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(series_assembly_test)
gtest_discover_tests(transfer_syntax_decoder_test)
gtest_discover_tests(hounsfield_converter_test)
gtest_discover_tests(volume_renderer_test)
gtest_discover_tests(transfer_function_manager_test)
gtest_discover_tests(mpr_renderer_test)
gtest_discover_tests(surface_renderer_test)
gtest_discover_tests(dicom_echo_scu_test)
gtest_discover_tests(dicom_find_scu_test DISCOVERY_TIMEOUT 60)

# Unit tests for DICOM Move SCU
add_executable(dicom_move_scu_test
    unit/dicom_move_scu_test.cpp
)

target_link_libraries(dicom_move_scu_test PRIVATE
    pacs_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(dicom_move_scu_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(dicom_move_scu_test DISCOVERY_TIMEOUT 60)

# Unit tests for PACS Config Manager
add_executable(pacs_config_manager_test
    unit/pacs_config_manager_test.cpp
)

target_link_libraries(pacs_config_manager_test PRIVATE
    pacs_service
    Qt6::Core
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(pacs_config_manager_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(pacs_config_manager_test DISCOVERY_TIMEOUT 60)

# Unit tests for DICOM Store SCP
add_executable(dicom_store_scp_test
    unit/dicom_store_scp_test.cpp
)

target_link_libraries(dicom_store_scp_test PRIVATE
    pacs_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(dicom_store_scp_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(dicom_store_scp_test DISCOVERY_TIMEOUT 60)

# Unit tests for Threshold Segmenter
add_executable(threshold_segmenter_test
    unit/threshold_segmenter_test.cpp
)

target_link_libraries(threshold_segmenter_test PRIVATE
    segmentation_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(threshold_segmenter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(threshold_segmenter_test DISCOVERY_TIMEOUT 60)

# Unit tests for Region Growing Segmenter
add_executable(region_growing_segmenter_test
    unit/region_growing_segmenter_test.cpp
)

target_link_libraries(region_growing_segmenter_test PRIVATE
    segmentation_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(region_growing_segmenter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(region_growing_segmenter_test DISCOVERY_TIMEOUT 60)

# Unit tests for Manual Segmentation Controller
add_executable(manual_segmentation_controller_test
    unit/manual_segmentation_controller_test.cpp
)

target_link_libraries(manual_segmentation_controller_test PRIVATE
    segmentation_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(manual_segmentation_controller_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(manual_segmentation_controller_test DISCOVERY_TIMEOUT 60)

# Unit tests for Label Manager
add_executable(label_manager_test
    unit/label_manager_test.cpp
)

target_link_libraries(label_manager_test PRIVATE
    segmentation_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(label_manager_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(label_manager_test DISCOVERY_TIMEOUT 60)

# Unit tests for Morphological Processor
add_executable(morphological_processor_test
    unit/morphological_processor_test.cpp
)

target_link_libraries(morphological_processor_test PRIVATE
    segmentation_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(morphological_processor_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(morphological_processor_test DISCOVERY_TIMEOUT 60)

# Unit tests for ROI Statistics Calculator
add_executable(roi_statistics_test
    unit/roi_statistics_test.cpp
)

target_link_libraries(roi_statistics_test PRIVATE
    measurement_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(roi_statistics_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(roi_statistics_test DISCOVERY_TIMEOUT 60)

# Unit tests for Volume Calculator
add_executable(volume_calculator_test
    unit/volume_calculator_test.cpp
)

target_link_directories(volume_calculator_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(volume_calculator_test PRIVATE
    measurement_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(volume_calculator_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS volume_calculator_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(volume_calculator_test DISCOVERY_TIMEOUT 60)

# Unit tests for Gaussian Smoother
add_executable(gaussian_smoother_test
    unit/gaussian_smoother_test.cpp
)

target_link_libraries(gaussian_smoother_test PRIVATE
    preprocessing_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(gaussian_smoother_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(gaussian_smoother_test DISCOVERY_TIMEOUT 60)

# Unit tests for Anisotropic Diffusion Filter
add_executable(anisotropic_diffusion_filter_test
    unit/anisotropic_diffusion_filter_test.cpp
)

target_link_libraries(anisotropic_diffusion_filter_test PRIVATE
    preprocessing_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(anisotropic_diffusion_filter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(anisotropic_diffusion_filter_test DISCOVERY_TIMEOUT 60)

# Unit tests for N4 Bias Corrector
add_executable(n4_bias_corrector_test
    unit/n4_bias_corrector_test.cpp
)

target_link_directories(n4_bias_corrector_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(n4_bias_corrector_test PRIVATE
    preprocessing_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(n4_bias_corrector_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(n4_bias_corrector_test DISCOVERY_TIMEOUT 120)

# Unit tests for Isotropic Resampler
add_executable(isotropic_resampler_test
    unit/isotropic_resampler_test.cpp
)

target_link_directories(isotropic_resampler_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(isotropic_resampler_test PRIVATE
    preprocessing_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(isotropic_resampler_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(isotropic_resampler_test DISCOVERY_TIMEOUT 60)

# Unit tests for Ellipse ROI
add_executable(ellipse_roi_test
    unit/ellipse_roi_test.cpp
)

target_link_directories(ellipse_roi_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(ellipse_roi_test PRIVATE
    measurement_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(ellipse_roi_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(ellipse_roi_test DISCOVERY_TIMEOUT 60)

# Unit tests for Area Measurement Tool
add_executable(area_measurement_tool_test
    unit/area_measurement_tool_test.cpp
)

target_link_directories(area_measurement_tool_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(area_measurement_tool_test PRIVATE
    measurement_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(area_measurement_tool_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS area_measurement_tool_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(area_measurement_tool_test DISCOVERY_TIMEOUT 60)

# Unit tests for MPR Coordinate Transformer
add_executable(mpr_coordinate_transformer_test
    unit/mpr_coordinate_transformer_test.cpp
)

target_link_directories(mpr_coordinate_transformer_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(mpr_coordinate_transformer_test PRIVATE
    segmentation_service
    render_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(mpr_coordinate_transformer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS mpr_coordinate_transformer_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(mpr_coordinate_transformer_test DISCOVERY_TIMEOUT 60)

# Unit tests for Report Generator
add_executable(report_generator_test
    unit/report_generator_test.cpp
)

target_link_directories(report_generator_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(report_generator_test PRIVATE
    export_service
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::PrintSupport
    Qt6::Test
    GTest::gtest
)

target_include_directories(report_generator_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(report_generator_test DISCOVERY_TIMEOUT 60)

# Unit tests for Data Exporter
add_executable(data_exporter_test
    unit/data_exporter_test.cpp
)

target_link_directories(data_exporter_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(data_exporter_test PRIVATE
    export_service
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Test
    GTest::gtest
)

target_include_directories(data_exporter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(data_exporter_test DISCOVERY_TIMEOUT 60)

# Unit tests for Measurement Serializer
add_executable(measurement_serializer_test
    unit/measurement_serializer_test.cpp
)

target_link_directories(measurement_serializer_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(measurement_serializer_test PRIVATE
    export_service
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Test
    GTest::gtest
)

target_include_directories(measurement_serializer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(measurement_serializer_test DISCOVERY_TIMEOUT 60)

# Unit tests for Oblique Reslice Renderer
add_executable(oblique_reslice_renderer_test
    unit/oblique_reslice_renderer_test.cpp
)

target_link_directories(oblique_reslice_renderer_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(oblique_reslice_renderer_test PRIVATE
    render_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(oblique_reslice_renderer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS oblique_reslice_renderer_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(oblique_reslice_renderer_test DISCOVERY_TIMEOUT 60)

# Unit tests for Mesh Exporter
add_executable(mesh_exporter_test
    unit/mesh_exporter_test.cpp
)

target_link_directories(mesh_exporter_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(mesh_exporter_test PRIVATE
    export_service
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Test
    GTest::gtest
)

target_include_directories(mesh_exporter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS mesh_exporter_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(mesh_exporter_test DISCOVERY_TIMEOUT 60)

# Unit tests for DICOM SR Writer
add_executable(dicom_sr_writer_test
    unit/dicom_sr_writer_test.cpp
)

target_link_directories(dicom_sr_writer_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(dicom_sr_writer_test PRIVATE
    export_service
    pacs_service
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Test
    GTest::gtest
)

target_include_directories(dicom_sr_writer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(dicom_sr_writer_test DISCOVERY_TIMEOUT 60)
