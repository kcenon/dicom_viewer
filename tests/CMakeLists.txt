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
