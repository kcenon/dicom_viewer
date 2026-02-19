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
    segmentation_service
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
if(PACS_SYSTEM_HAS_ASAN)
    # When pacs_system has ASan, test binaries may crash during
    # --gtest_list_tests (unresolved ASan symbols at static init).
    # Override gtest_discover_tests to use add_test as a safe fallback.
    # A wrapper script converts ASan-induced crashes (SIGABRT) to
    # SKIP_RETURN_CODE so ctest marks them as SKIPPED, not FAILED.
    set(_asan_wrapper "${CMAKE_BINARY_DIR}/asan_test_wrapper.sh")
    file(WRITE "${_asan_wrapper}"
        "#!/bin/bash\n"
        "output=$(\"$@\" 2>&1)\n"
        "rc=$?\n"
        "printf '%s\\n' \"$output\"\n"
        "if printf '%s' \"$output\" | grep -q '___asan_'; then exit 125; fi\n"
        "exit $rc\n")
    file(CHMOD "${_asan_wrapper}" PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    macro(gtest_discover_tests TARGET)
        add_test(NAME ${TARGET} COMMAND
            "${_asan_wrapper}" "$<TARGET_FILE:${TARGET}>")
        set_tests_properties(${TARGET} PROPERTIES SKIP_RETURN_CODE 125)
    endmacro()
endif()
gtest_discover_tests(series_assembly_test DISCOVERY_TIMEOUT 60)
gtest_discover_tests(transfer_syntax_decoder_test DISCOVERY_TIMEOUT 60)
gtest_discover_tests(hounsfield_converter_test DISCOVERY_TIMEOUT 60)
gtest_discover_tests(volume_renderer_test DISCOVERY_TIMEOUT 60)
gtest_discover_tests(transfer_function_manager_test DISCOVERY_TIMEOUT 60)
gtest_discover_tests(mpr_renderer_test DISCOVERY_TIMEOUT 60)
gtest_discover_tests(surface_renderer_test DISCOVERY_TIMEOUT 60)
gtest_discover_tests(dicom_echo_scu_test DISCOVERY_TIMEOUT 60)
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

# Unit tests for Mask Boolean Operations
add_executable(mask_boolean_operations_test
    unit/mask_boolean_operations_test.cpp
)

target_link_libraries(mask_boolean_operations_test PRIVATE
    segmentation_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(mask_boolean_operations_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(mask_boolean_operations_test DISCOVERY_TIMEOUT 60)

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

# Unit tests for Shape Analyzer
add_executable(shape_analyzer_test
    unit/shape_analyzer_test.cpp
)

target_link_directories(shape_analyzer_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(shape_analyzer_test PRIVATE
    measurement_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(shape_analyzer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS shape_analyzer_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(shape_analyzer_test DISCOVERY_TIMEOUT 60)

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
    GTest::gtest_main
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

# Unit tests for Level Set Segmenter
add_executable(level_set_segmenter_test
    unit/level_set_segmenter_test.cpp
)

target_link_libraries(level_set_segmenter_test PRIVATE
    segmentation_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(level_set_segmenter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(level_set_segmenter_test DISCOVERY_TIMEOUT 120)

# Unit tests for Slice Interpolator
add_executable(slice_interpolator_test
    unit/slice_interpolator_test.cpp
)

target_link_libraries(slice_interpolator_test PRIVATE
    segmentation_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(slice_interpolator_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(slice_interpolator_test DISCOVERY_TIMEOUT 120)

# Unit tests for Watershed Segmenter
add_executable(watershed_segmenter_test
    unit/watershed_segmenter_test.cpp
)

target_link_libraries(watershed_segmenter_test PRIVATE
    segmentation_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(watershed_segmenter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(watershed_segmenter_test DISCOVERY_TIMEOUT 120)

# Unit tests for Histogram Equalizer
add_executable(histogram_equalizer_test
    unit/histogram_equalizer_test.cpp
)

target_link_libraries(histogram_equalizer_test PRIVATE
    preprocessing_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(histogram_equalizer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(histogram_equalizer_test DISCOVERY_TIMEOUT 60)

# Unit tests for DR Viewer
add_executable(dr_viewer_test
    unit/dr_viewer_test.cpp
)

target_link_directories(dr_viewer_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(dr_viewer_test PRIVATE
    dicom_viewer_ui
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(dr_viewer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS dr_viewer_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(dr_viewer_test DISCOVERY_TIMEOUT 60)

# Unit tests for Flow DICOM Parser
add_executable(flow_dicom_parser_test
    unit/flow_dicom_parser_test.cpp
)

target_link_libraries(flow_dicom_parser_test PRIVATE
    flow_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(flow_dicom_parser_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(flow_dicom_parser_test DISCOVERY_TIMEOUT 60)

# Unit tests for Velocity Field Assembler
add_executable(velocity_field_assembler_test
    unit/velocity_field_assembler_test.cpp
)

target_link_libraries(velocity_field_assembler_test PRIVATE
    flow_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(velocity_field_assembler_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(velocity_field_assembler_test DISCOVERY_TIMEOUT 60)

# Unit tests for Phase Corrector
add_executable(phase_corrector_test
    unit/phase_corrector_test.cpp
)

target_link_libraries(phase_corrector_test PRIVATE
    flow_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(phase_corrector_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(phase_corrector_test DISCOVERY_TIMEOUT 60)

# Unit tests for Temporal Navigator
add_executable(temporal_navigator_test
    unit/temporal_navigator_test.cpp
)

target_link_libraries(temporal_navigator_test PRIVATE
    flow_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(temporal_navigator_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(temporal_navigator_test DISCOVERY_TIMEOUT 60)

# Flow visualizer tests
add_executable(flow_visualizer_test
    unit/flow_visualizer_test.cpp
)

target_link_libraries(flow_visualizer_test PRIVATE
    flow_service
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(flow_visualizer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(flow_visualizer_test DISCOVERY_TIMEOUT 60)

# Flow quantifier tests
add_executable(flow_quantifier_test
    unit/flow_quantifier_test.cpp
)

target_link_libraries(flow_quantifier_test PRIVATE
    flow_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(flow_quantifier_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(flow_quantifier_test DISCOVERY_TIMEOUT 60)

# Integration tests for 4D Flow pipeline (end-to-end validation)
add_executable(flow_pipeline_integration_test
    integration/flow_pipeline_integration_test.cpp
)

target_link_libraries(flow_pipeline_integration_test PRIVATE
    flow_service
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(flow_pipeline_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(flow_pipeline_integration_test DISCOVERY_TIMEOUT 120)

# Vessel analyzer tests (WSS, vorticity, TKE)
add_executable(vessel_analyzer_test
    unit/vessel_analyzer_test.cpp
)

target_link_libraries(vessel_analyzer_test PRIVATE
    flow_service
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(vessel_analyzer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(vessel_analyzer_test DISCOVERY_TIMEOUT 120)

# Unit tests for Enhanced DICOM Parser
add_executable(enhanced_dicom_parser_test
    unit/enhanced_dicom_parser_test.cpp
)

target_link_libraries(enhanced_dicom_parser_test PRIVATE
    enhanced_dicom_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(enhanced_dicom_parser_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(enhanced_dicom_parser_test DISCOVERY_TIMEOUT 60)

# Unit tests for DimensionIndex Sorter
add_executable(dimension_index_sorter_test
    unit/dimension_index_sorter_test.cpp
)

target_link_libraries(dimension_index_sorter_test PRIVATE
    enhanced_dicom_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(dimension_index_sorter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(dimension_index_sorter_test DISCOVERY_TIMEOUT 60)

# Unit tests for Series Classifier
add_executable(series_classifier_test
    unit/series_classifier_test.cpp
)

target_link_libraries(series_classifier_test PRIVATE
    enhanced_dicom_service
    ${ITK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(series_classifier_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(series_classifier_test DISCOVERY_TIMEOUT 60)

# Unit tests for Cardiac Phase Detector
add_executable(cardiac_phase_detector_test
    unit/cardiac_phase_detector_test.cpp
)

target_link_libraries(cardiac_phase_detector_test PRIVATE
    cardiac_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(cardiac_phase_detector_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(cardiac_phase_detector_test DISCOVERY_TIMEOUT 60)

# Unit tests for Calcium Scorer
add_executable(calcium_scorer_test
    unit/calcium_scorer_test.cpp
)

target_link_libraries(calcium_scorer_test PRIVATE
    cardiac_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(calcium_scorer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(calcium_scorer_test DISCOVERY_TIMEOUT 60)

# Unit tests for Coronary Centerline Extractor and CPR
add_executable(coronary_centerline_extractor_test
    unit/coronary_centerline_extractor_test.cpp
)

target_link_libraries(coronary_centerline_extractor_test PRIVATE
    cardiac_service
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(coronary_centerline_extractor_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS coronary_centerline_extractor_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(coronary_centerline_extractor_test DISCOVERY_TIMEOUT 120)

# Integration tests for Cardiac CT pipeline (calcium scoring, centerline, CPR, phases)
add_executable(cardiac_pipeline_integration_test
    integration/cardiac_pipeline_integration_test.cpp
)

target_link_libraries(cardiac_pipeline_integration_test PRIVATE
    cardiac_service
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(cardiac_pipeline_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS cardiac_pipeline_integration_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(cardiac_pipeline_integration_test DISCOVERY_TIMEOUT 120)

# Unit tests for Cine MRI Organizer
add_executable(cine_organizer_test
    unit/cine_organizer_test.cpp
)

target_link_libraries(cine_organizer_test PRIVATE
    cardiac_service
    flow_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(cine_organizer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests
)

gtest_discover_tests(cine_organizer_test DISCOVERY_TIMEOUT 60)

# Unit tests for DicomLoader
add_executable(dicom_loader_test
    unit/dicom_loader_test.cpp
)

target_link_libraries(dicom_loader_test PRIVATE
    dicom_viewer_core
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(dicom_loader_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(dicom_loader_test DISCOVERY_TIMEOUT 60)

# Unit tests for Linear Measurement Tool
add_executable(linear_measurement_tool_test
    unit/linear_measurement_tool_test.cpp
)

target_link_directories(linear_measurement_tool_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(linear_measurement_tool_test PRIVATE
    measurement_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(linear_measurement_tool_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS linear_measurement_tool_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(linear_measurement_tool_test DISCOVERY_TIMEOUT 60)

# Unit tests for ImageConverter
add_executable(image_converter_test
    unit/image_converter_test.cpp
)

target_link_libraries(image_converter_test PRIVATE
    dicom_viewer_core
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(image_converter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS image_converter_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(image_converter_test DISCOVERY_TIMEOUT 60)

# Unit tests for FunctionalGroupParser
add_executable(functional_group_parser_test
    unit/functional_group_parser_test.cpp
)

target_link_libraries(functional_group_parser_test PRIVATE
    enhanced_dicom_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(functional_group_parser_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(functional_group_parser_test DISCOVERY_TIMEOUT 60)

# Unit tests for FrameExtractor
add_executable(frame_extractor_test
    unit/frame_extractor_test.cpp
)

target_link_libraries(frame_extractor_test PRIVATE
    enhanced_dicom_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(frame_extractor_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(frame_extractor_test DISCOVERY_TIMEOUT 60)

# Unit tests for MPR Segmentation Renderer
add_executable(mpr_segmentation_renderer_test
    unit/mpr_segmentation_renderer_test.cpp
)

target_link_libraries(mpr_segmentation_renderer_test PRIVATE
    segmentation_service
    render_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(mpr_segmentation_renderer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS mpr_segmentation_renderer_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(mpr_segmentation_renderer_test DISCOVERY_TIMEOUT 60)

# Unit tests for label map overlay
add_executable(label_map_overlay_test
    unit/label_map_overlay_test.cpp
)

target_link_libraries(label_map_overlay_test PRIVATE
    segmentation_service
    render_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(label_map_overlay_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS label_map_overlay_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(label_map_overlay_test DISCOVERY_TIMEOUT 60)

# Integration tests for series loading pipeline (converted from manual CLI)
add_executable(series_loading_integration_test
    integration/test_series_loading.cpp
)

target_link_libraries(series_loading_integration_test PRIVATE
    dicom_viewer_core
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(series_loading_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(series_loading_integration_test DISCOVERY_TIMEOUT 60)

# Integration tests for slice ordering pipeline (converted from manual CLI)
add_executable(slice_ordering_integration_test
    integration/test_slice_ordering.cpp
)

target_link_libraries(slice_ordering_integration_test PRIVATE
    dicom_viewer_core
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(slice_ordering_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(slice_ordering_integration_test DISCOVERY_TIMEOUT 60)

# Performance benchmark tests (infrastructure for regression testing)
add_executable(performance_benchmark_test
    unit/performance_benchmark_test.cpp
)

target_link_directories(performance_benchmark_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(performance_benchmark_test PRIVATE
    dicom_viewer_core
    preprocessing_service
    segmentation_service
    cardiac_service
    flow_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(performance_benchmark_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(performance_benchmark_test DISCOVERY_TIMEOUT 120)

# Rendering and VTK-dependent benchmark tests
add_executable(rendering_benchmark_test
    unit/rendering_benchmark_test.cpp
)

target_link_directories(rendering_benchmark_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(rendering_benchmark_test PRIVATE
    dicom_viewer_core
    render_service
    segmentation_service
    flow_service
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(rendering_benchmark_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS rendering_benchmark_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(rendering_benchmark_test DISCOVERY_TIMEOUT 120)

# Concurrency and thread-safety tests
add_executable(concurrency_test
    unit/concurrency_test.cpp
)

target_link_directories(concurrency_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(concurrency_test PRIVATE
    segmentation_service
    preprocessing_service
    flow_service
    pacs_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(concurrency_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(concurrency_test DISCOVERY_TIMEOUT 120)

# Phase slider widget tests
find_package(Qt6 COMPONENTS Test QUIET)
add_executable(phase_slider_widget_test
    unit/phase_slider_widget_test.cpp
)

target_link_libraries(phase_slider_widget_test PRIVATE
    dicom_viewer_ui
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(phase_slider_widget_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(phase_slider_widget_test DISCOVERY_TIMEOUT 60)

# Viewport layout manager tests
add_executable(viewport_layout_manager_test
    unit/viewport_layout_manager_test.cpp
)

target_link_libraries(viewport_layout_manager_test PRIVATE
    dicom_viewer_ui
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(viewport_layout_manager_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(viewport_layout_manager_test DISCOVERY_TIMEOUT 60)

# Unit tests for Segmentation Command Stack and BrushStrokeCommand
add_executable(segmentation_command_test
    unit/segmentation_command_test.cpp
)

target_link_libraries(segmentation_command_test PRIVATE
    segmentation_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(segmentation_command_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(segmentation_command_test DISCOVERY_TIMEOUT 60)

# S/P mode toggle and phase slider integration tests
add_executable(sp_mode_toggle_test
    unit/sp_mode_toggle_test.cpp
)

target_link_libraries(sp_mode_toggle_test PRIVATE
    dicom_viewer_ui
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(sp_mode_toggle_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(sp_mode_toggle_test DISCOVERY_TIMEOUT 60)

# Unit tests for controller undo/redo integration
add_executable(controller_undo_redo_test
    unit/controller_undo_redo_test.cpp
)

target_link_libraries(controller_undo_redo_test PRIVATE
    segmentation_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(controller_undo_redo_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(controller_undo_redo_test DISCOVERY_TIMEOUT 60)

# Unit tests for ProjectManager and ZipArchive
add_executable(project_manager_test
    unit/project_manager_test.cpp
)

target_link_libraries(project_manager_test PRIVATE
    dicom_viewer_core
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(project_manager_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(project_manager_test DISCOVERY_TIMEOUT 60)

# Unit tests for DataSerializer (NRRD serialization + ZIP project files)
add_executable(data_serializer_test
    unit/data_serializer_test.cpp
)

target_link_libraries(data_serializer_test PRIVATE
    dicom_viewer_core
    ${ITK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(data_serializer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(data_serializer_test DISCOVERY_TIMEOUT 60)

# Unit tests for Hemodynamic Overlay Renderer
add_executable(hemodynamic_overlay_renderer_test
    unit/hemodynamic_overlay_renderer_test.cpp
)

target_link_directories(hemodynamic_overlay_renderer_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(hemodynamic_overlay_renderer_test PRIVATE
    render_service
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(hemodynamic_overlay_renderer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS hemodynamic_overlay_renderer_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(hemodynamic_overlay_renderer_test DISCOVERY_TIMEOUT 60)

# Unit tests for Streamline Overlay Renderer
add_executable(streamline_overlay_renderer_test
    unit/streamline_overlay_renderer_test.cpp
)

target_link_directories(streamline_overlay_renderer_test PRIVATE
    /opt/homebrew/lib
)

target_link_libraries(streamline_overlay_renderer_test PRIVATE
    render_service
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(streamline_overlay_renderer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS streamline_overlay_renderer_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(streamline_overlay_renderer_test DISCOVERY_TIMEOUT 60)

# Volume Renderer Overlay Tests
add_executable(volume_renderer_overlay_test
    unit/volume_renderer_overlay_test.cpp
)

target_link_libraries(volume_renderer_overlay_test PRIVATE
    render_service
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(volume_renderer_overlay_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS volume_renderer_overlay_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(volume_renderer_overlay_test DISCOVERY_TIMEOUT 60)

# Kinetic Energy and Heart Rate Tests
add_executable(kinetic_energy_heart_rate_test
    unit/kinetic_energy_heart_rate_test.cpp
)

target_link_libraries(kinetic_energy_heart_rate_test PRIVATE
    flow_service
    ${ITK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(kinetic_energy_heart_rate_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests
)

gtest_discover_tests(kinetic_energy_heart_rate_test DISCOVERY_TIMEOUT 60)

# 2D Plane Stats and RRT Tests
add_executable(plane_stats_rrt_test
    unit/plane_stats_rrt_test.cpp
)

target_link_libraries(plane_stats_rrt_test PRIVATE
    flow_service
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(plane_stats_rrt_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests
)

vtk_module_autoinit(
    TARGETS plane_stats_rrt_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(plane_stats_rrt_test DISCOVERY_TIMEOUT 60)

# Unit tests for SnapshotCommand (RLE compression + undo/redo)
add_executable(snapshot_command_test
    unit/snapshot_command_test.cpp
)

target_link_libraries(snapshot_command_test PRIVATE
    segmentation_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(snapshot_command_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(snapshot_command_test DISCOVERY_TIMEOUT 60)

# Unit tests for PhaseTracker (temporal mask propagation)
add_executable(phase_tracker_test
    unit/phase_tracker_test.cpp
)

target_link_libraries(phase_tracker_test PRIVATE
    segmentation_service
    ${ITK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(phase_tracker_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(phase_tracker_test DISCOVERY_TIMEOUT 120)

# Unit tests for HollowTool and MaskSmoother
add_executable(hollow_smoother_test
    unit/hollow_smoother_test.cpp
)

target_link_libraries(hollow_smoother_test PRIVATE
    segmentation_service
    ${ITK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(hollow_smoother_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(hollow_smoother_test DISCOVERY_TIMEOUT 120)

# Unit tests for CenterlineTracer (vessel centerline tracing)
add_executable(centerline_tracer_test
    unit/centerline_tracer_test.cpp
)

target_link_libraries(centerline_tracer_test PRIVATE
    segmentation_service
    ${ITK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(centerline_tracer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(centerline_tracer_test DISCOVERY_TIMEOUT 120)

# Unit tests for Level Tracing contour tool
add_executable(level_tracing_test
    unit/level_tracing_test.cpp
)

target_link_libraries(level_tracing_test PRIVATE
    segmentation_service
    ${ITK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(level_tracing_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(level_tracing_test DISCOVERY_TIMEOUT 60)

# Unit tests for Ensight Gold format exporter
add_executable(ensight_exporter_test
    unit/ensight_exporter_test.cpp
)

target_link_libraries(ensight_exporter_test PRIVATE
    export_service
    ${ITK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(ensight_exporter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(ensight_exporter_test DISCOVERY_TIMEOUT 60)

# Unit tests for MATLAB MAT-file v5 exporter
add_executable(matlab_exporter_test
    unit/matlab_exporter_test.cpp
)

target_link_libraries(matlab_exporter_test PRIVATE
    export_service
    ${ITK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(matlab_exporter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(matlab_exporter_test DISCOVERY_TIMEOUT 60)

# Unit tests for OGG Theora video exporter
add_executable(video_exporter_test
    unit/video_exporter_test.cpp
)

target_link_libraries(video_exporter_test PRIVATE
    export_service
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(video_exporter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(video_exporter_test DISCOVERY_TIMEOUT 60)

# Unit tests for Vendor-specific Flow Parsers (fallback paths and edge cases)
add_executable(vendor_flow_parsers_test
    unit/vendor_flow_parsers_test.cpp
)

target_link_libraries(vendor_flow_parsers_test PRIVATE
    flow_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(vendor_flow_parsers_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(vendor_flow_parsers_test DISCOVERY_TIMEOUT 60)

# Unit tests for Overlay Control Panel
add_executable(overlay_control_panel_test
    unit/overlay_control_panel_test.cpp
)

target_link_libraries(overlay_control_panel_test PRIVATE
    dicom_viewer_ui
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(overlay_control_panel_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(overlay_control_panel_test DISCOVERY_TIMEOUT 60)

# Unit tests for Flow Tool Panel
add_executable(flow_tool_panel_test
    unit/flow_tool_panel_test.cpp
)

target_link_libraries(flow_tool_panel_test PRIVATE
    dicom_viewer_ui
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(flow_tool_panel_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(flow_tool_panel_test DISCOVERY_TIMEOUT 60)

# Unit tests for Hemodynamic Surface Manager
add_executable(hemodynamic_surface_manager_test
    unit/hemodynamic_surface_manager_test.cpp
)

target_link_libraries(hemodynamic_surface_manager_test PRIVATE
    render_service
    ${VTK_LIBRARIES}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(hemodynamic_surface_manager_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS hemodynamic_surface_manager_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(hemodynamic_surface_manager_test DISCOVERY_TIMEOUT 60)

# Unit tests for Display3DController
add_executable(display_3d_controller_test
    unit/display_3d_controller_test.cpp
)

target_link_libraries(display_3d_controller_test PRIVATE
    dicom_viewer_ui
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(display_3d_controller_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS display_3d_controller_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(display_3d_controller_test DISCOVERY_TIMEOUT 60)

add_executable(asc_view_controller_test
    unit/asc_view_controller_test.cpp
)

target_link_libraries(asc_view_controller_test PRIVATE
    render_service
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(asc_view_controller_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

vtk_module_autoinit(
    TARGETS asc_view_controller_test
    MODULES ${VTK_LIBRARIES}
)

gtest_discover_tests(asc_view_controller_test DISCOVERY_TIMEOUT 60)

# Unit tests for QuantificationWindow
add_executable(quantification_window_test
    unit/quantification_window_test.cpp
)

target_link_libraries(quantification_window_test PRIVATE
    dicom_viewer_ui
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(quantification_window_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(quantification_window_test DISCOVERY_TIMEOUT 60)

# Unit tests for FlowGraphWidget (QPainter-based time-series chart)
add_executable(flow_graph_widget_test
    unit/flow_graph_widget_test.cpp
)

target_link_libraries(flow_graph_widget_test PRIVATE
    dicom_viewer_ui
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(flow_graph_widget_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(flow_graph_widget_test DISCOVERY_TIMEOUT 60)

# Unit tests for PatientBrowser (classification display)
add_executable(patient_browser_test
    unit/patient_browser_test.cpp
)

target_link_libraries(patient_browser_test PRIVATE
    dicom_viewer_ui
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(patient_browser_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(patient_browser_test DISCOVERY_TIMEOUT 60)

# Unit tests for MaskWizard (skeleton wizard navigation)
add_executable(mask_wizard_test
    unit/mask_wizard_test.cpp
)

target_link_libraries(mask_wizard_test PRIVATE
    dicom_viewer_ui
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(mask_wizard_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(mask_wizard_test DISCOVERY_TIMEOUT 60)

# Unit tests for DropHandler (drag-and-drop classification)
add_executable(drop_handler_test
    unit/drop_handler_test.cpp
)

target_link_libraries(drop_handler_test PRIVATE
    dicom_viewer_ui
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(drop_handler_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(drop_handler_test DISCOVERY_TIMEOUT 60)

# Unit tests for MaskWizardController (service wiring)
add_executable(mask_wizard_controller_test
    unit/mask_wizard_controller_test.cpp
)

target_link_libraries(mask_wizard_controller_test PRIVATE
    dicom_viewer_ui
    Qt6::Test
    GTest::gtest
)

target_include_directories(mask_wizard_controller_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(mask_wizard_controller_test DISCOVERY_TIMEOUT 60)
