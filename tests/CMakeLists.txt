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

# VTK module autoinit for tests
vtk_module_autoinit(
    TARGETS volume_renderer_test transfer_function_manager_test
    MODULES ${VTK_LIBRARIES}
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(series_assembly_test)
gtest_discover_tests(transfer_syntax_decoder_test)
gtest_discover_tests(hounsfield_converter_test)
gtest_discover_tests(volume_renderer_test)
gtest_discover_tests(transfer_function_manager_test)
