# Server build configuration (primary build target)
cmake_minimum_required(VERSION 3.20)

# Collect server source files
set(SERVER_SOURCES
    src/main.cpp
    src/api/api_server.cpp
    src/api/auth_routes.cpp
    src/api/cardiac_routes.cpp
    src/api/export_routes.cpp
    src/api/flow_routes.cpp
    src/api/health_routes.cpp
    src/api/measurement_routes.cpp
    src/api/pacs_routes.cpp
    src/api/render_routes.cpp
    src/api/segmentation_routes.cpp
    src/api/session_routes.cpp
    src/api/study_routes.cpp
)

# Server executable
add_executable(dicom_viewer ${SERVER_SOURCES})

target_include_directories(dicom_viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${PACS_SYSTEM_INCLUDE_DIR}
)

# Inherit Crow + Asio include paths from render_service
get_target_property(RENDER_INCLUDES render_service INTERFACE_INCLUDE_DIRECTORIES)
if(RENDER_INCLUDES)
    target_include_directories(dicom_viewer PRIVATE ${RENDER_INCLUDES})
endif()

target_link_libraries(dicom_viewer PRIVATE
    render_service
    config_service
    store_service
    pacs_service
    spdlog::spdlog
    fmt::fmt
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(TARGET jwt-cpp::jwt-cpp)
    target_link_libraries(dicom_viewer PRIVATE jwt-cpp::jwt-cpp)
endif()

# VTK module autoinit
vtk_module_autoinit(
    TARGETS dicom_viewer render_service
    MODULES ${VTK_LIBRARIES}
)

install(TARGETS dicom_viewer
    RUNTIME DESTINATION bin
)
