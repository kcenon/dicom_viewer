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

# macOS platform configuration
if(APPLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS version" FORCE)
    endif()

    add_compile_definitions(_USE_MATH_DEFINES)
    add_compile_options(-Wno-deprecated-declarations)
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
set(VTK_REQUIRED_COMPONENTS
    CommonCore
    CommonDataModel
    CommonExecutionModel
    CommonTransforms
    FiltersCore
    FiltersFlowPaths
    FiltersGeneral
    FiltersGeometry
    FiltersSources
    ImagingCore
    InteractionStyle
    InteractionWidgets
    IOGeometry
    IOImage
    RenderingCore
    RenderingOpenGL2
    RenderingVolumeOpenGL2
)
find_package(VTK REQUIRED COMPONENTS ${VTK_REQUIRED_COMPONENTS})

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

# GDCM library for low-level DICOM parsing (Enhanced DICOM, raw tag access)
find_package(GDCM REQUIRED)
include(${GDCM_USE_FILE})

# pacs_system for DICOM network operations (required)
# DCMTK migration completed - now using pacs_system exclusively

# pacs_system local path configuration
# Set PACS_SYSTEM_ROOT to the pacs_system source directory
if(NOT PACS_SYSTEM_ROOT)
    # Default to sibling directory
    set(PACS_SYSTEM_ROOT "${CMAKE_SOURCE_DIR}/../pacs_system" CACHE PATH "Path to pacs_system source")
endif()

if(NOT PACS_SYSTEM_BUILD_DIR)
    set(PACS_SYSTEM_BUILD_DIR "${PACS_SYSTEM_ROOT}/build" CACHE PATH "Path to pacs_system build directory")
endif()

# common_system is a dependency of pacs_system
if(NOT COMMON_SYSTEM_ROOT)
    set(COMMON_SYSTEM_ROOT "${CMAKE_SOURCE_DIR}/../common_system" CACHE PATH "Path to common_system source")
endif()
if(EXISTS "${COMMON_SYSTEM_ROOT}/include")
    set(COMMON_SYSTEM_INCLUDE_DIR "${COMMON_SYSTEM_ROOT}/include")
    message(STATUS "Found common_system at: ${COMMON_SYSTEM_ROOT}")
endif()

# thread_system is a dependency of pacs_system
if(NOT THREAD_SYSTEM_ROOT)
    set(THREAD_SYSTEM_ROOT "${CMAKE_SOURCE_DIR}/../thread_system" CACHE PATH "Path to thread_system source")
endif()
if(EXISTS "${THREAD_SYSTEM_ROOT}/include")
    set(THREAD_SYSTEM_INCLUDE_DIR "${THREAD_SYSTEM_ROOT}/include")
    message(STATUS "Found thread_system at: ${THREAD_SYSTEM_ROOT}")
endif()

# logger_system for ecosystem logging
if(NOT LOGGER_SYSTEM_ROOT)
    set(LOGGER_SYSTEM_ROOT "${CMAKE_SOURCE_DIR}/../logger_system" CACHE PATH "Path to logger_system source")
endif()
if(EXISTS "${LOGGER_SYSTEM_ROOT}/include")
    set(LOGGER_SYSTEM_INCLUDE_DIR "${LOGGER_SYSTEM_ROOT}/include")
    message(STATUS "Found logger_system at: ${LOGGER_SYSTEM_ROOT}")
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

    # Find optional pacs_system extension libraries
    find_library(PACS_SECURITY_LIB pacs_security HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(PACS_STORAGE_LIB pacs_storage HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(PACS_WEB_LIB pacs_web HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(PACS_CLIENT_LIB pacs_client HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(PACS_WORKFLOW_LIB pacs_workflow HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(PACS_MONITORING_LIB pacs_monitoring HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(PACS_AI_LIB pacs_ai HINTS ${PACS_SYSTEM_LIB_DIR})

    # Find dependency libraries from pacs_system
    find_library(CONTAINER_SYSTEM_LIB container_system HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(NETWORK_SYSTEM_LIB NetworkSystem HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(LOGGER_SYSTEM_LIB LoggerSystem HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(THREAD_BASE_LIB thread_base HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(DATABASE_LIB database HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(UTILITIES_LIB utilities HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(MONITORING_SYSTEM_LIB monitoring_system HINTS ${PACS_SYSTEM_LIB_DIR})
    find_library(THREAD_CORE_LIB thread_core HINTS ${PACS_SYSTEM_LIB_DIR})

    if(PACS_CORE_LIB AND PACS_NETWORK_LIB AND PACS_SERVICES_LIB)
        set(PACS_SYSTEM_FOUND TRUE)
        message(STATUS "pacs_system libraries found:")
        message(STATUS "  - pacs_core: ${PACS_CORE_LIB}")
        message(STATUS "  - pacs_network: ${PACS_NETWORK_LIB}")
        message(STATUS "  - pacs_services: ${PACS_SERVICES_LIB}")

        # Build include directories list for pacs_system
        set(PACS_INCLUDE_DIRS "${PACS_SYSTEM_INCLUDE_DIR}")
        if(COMMON_SYSTEM_INCLUDE_DIR)
            list(APPEND PACS_INCLUDE_DIRS "${COMMON_SYSTEM_INCLUDE_DIR}")
        endif()
        if(THREAD_SYSTEM_INCLUDE_DIR)
            list(APPEND PACS_INCLUDE_DIRS "${THREAD_SYSTEM_INCLUDE_DIR}")
        endif()

        # Create imported target for pacs_system
        add_library(pacs::core STATIC IMPORTED)
        set_target_properties(pacs::core PROPERTIES
            IMPORTED_LOCATION "${PACS_CORE_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${PACS_INCLUDE_DIRS}"
        )

        add_library(pacs::network STATIC IMPORTED)
        set_target_properties(pacs::network PROPERTIES
            IMPORTED_LOCATION "${PACS_NETWORK_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${PACS_INCLUDE_DIRS}"
        )

        add_library(pacs::services STATIC IMPORTED)
        set_target_properties(pacs::services PROPERTIES
            IMPORTED_LOCATION "${PACS_SERVICES_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${PACS_INCLUDE_DIRS}"
        )

        if(PACS_ENCODING_LIB)
            add_library(pacs::encoding STATIC IMPORTED)
            set_target_properties(pacs::encoding PROPERTIES
                IMPORTED_LOCATION "${PACS_ENCODING_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${PACS_INCLUDE_DIRS}"
            )
            if(APPLE)
                set_property(TARGET pacs::encoding APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES iconv)
            endif()

            # pacs_encoding codec dependencies (transitive from pacs_system)
            find_package(JPEG QUIET)
            if(JPEG_FOUND)
                set_property(TARGET pacs::encoding APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES JPEG::JPEG)
            endif()

            find_package(OpenJPEG QUIET)
            if(OpenJPEG_FOUND AND TARGET openjp2)
                set_property(TARGET pacs::encoding APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES openjp2)
            else()
                find_library(OPENJP2_LIB openjp2)
                if(OPENJP2_LIB)
                    set_property(TARGET pacs::encoding APPEND PROPERTY
                        INTERFACE_LINK_LIBRARIES "${OPENJP2_LIB}")
                endif()
            endif()

            find_package(openjph CONFIG QUIET)
            if(openjph_FOUND AND TARGET openjph::openjph)
                set_property(TARGET pacs::encoding APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES openjph::openjph)
            else()
                find_library(OPENJPH_LIB openjph)
                if(OPENJPH_LIB)
                    set_property(TARGET pacs::encoding APPEND PROPERTY
                        INTERFACE_LINK_LIBRARIES "${OPENJPH_LIB}")
                endif()
            endif()

            find_package(charls CONFIG QUIET)
            if(charls_FOUND AND TARGET charls::charls)
                set_property(TARGET pacs::encoding APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES charls::charls)
            else()
                find_library(CHARLS_LIB charls)
                if(CHARLS_LIB)
                    set_property(TARGET pacs::encoding APPEND PROPERTY
                        INTERFACE_LINK_LIBRARIES "${CHARLS_LIB}")
                endif()
            endif()
        endif()

        if(PACS_INTEGRATION_LIB)
            add_library(pacs::integration STATIC IMPORTED)
            set_target_properties(pacs::integration PROPERTIES
                IMPORTED_LOCATION "${PACS_INTEGRATION_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${PACS_INCLUDE_DIRS}"
            )
        endif()

        # Create optional extension module targets
        if(PACS_SECURITY_LIB)
            # pacs_security depends on OpenSSL for TLS syslog transport
            find_package(OpenSSL QUIET)
            add_library(pacs::security STATIC IMPORTED)
            set_target_properties(pacs::security PROPERTIES
                IMPORTED_LOCATION "${PACS_SECURITY_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${PACS_INCLUDE_DIRS}"
            )
            if(OpenSSL_FOUND)
                set_property(TARGET pacs::security APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
            endif()
            message(STATUS "  - pacs_security: ${PACS_SECURITY_LIB}")
        endif()

        if(PACS_STORAGE_LIB)
            add_library(pacs::storage STATIC IMPORTED)
            set_target_properties(pacs::storage PROPERTIES
                IMPORTED_LOCATION "${PACS_STORAGE_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${PACS_INCLUDE_DIRS}"
            )
            # pacs_storage depends on database, integrated_database, sqlite3
            find_library(INTEGRATED_DB_LIB integrated_database HINTS ${PACS_SYSTEM_LIB_DIR})
            if(DATABASE_LIB)
                set_property(TARGET pacs::storage APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES "${DATABASE_LIB}")
            endif()
            if(INTEGRATED_DB_LIB)
                set_property(TARGET pacs::storage APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES "${INTEGRATED_DB_LIB}")
            endif()
            find_package(SQLite3 QUIET)
            if(SQLite3_FOUND)
                set_property(TARGET pacs::storage APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES SQLite::SQLite3)
            else()
                find_library(SQLITE3_LIB sqlite3)
                if(SQLITE3_LIB)
                    set_property(TARGET pacs::storage APPEND PROPERTY
                        INTERFACE_LINK_LIBRARIES "${SQLITE3_LIB}")
                endif()
            endif()
            if(UTILITIES_LIB)
                set_property(TARGET pacs::storage APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES "${UTILITIES_LIB}")
            endif()
            message(STATUS "  - pacs_storage: ${PACS_STORAGE_LIB}")
        endif()

        if(PACS_WEB_LIB)
            add_library(pacs::web STATIC IMPORTED)
            set_target_properties(pacs::web PROPERTIES
                IMPORTED_LOCATION "${PACS_WEB_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${PACS_INCLUDE_DIRS}"
            )
            # pacs_web transitive dependencies (storage, client, monitoring)
            if(PACS_STORAGE_LIB AND TARGET pacs::storage)
                set_property(TARGET pacs::web APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES pacs::storage)
            endif()
            if(PACS_CLIENT_LIB AND TARGET pacs::client)
                set_property(TARGET pacs::web APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES pacs::client)
            endif()
            if(PACS_MONITORING_LIB AND TARGET pacs::monitoring)
                set_property(TARGET pacs::web APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES pacs::monitoring)
            endif()
            message(STATUS "  - pacs_web: ${PACS_WEB_LIB}")
        endif()

        if(PACS_CLIENT_LIB)
            add_library(pacs::client STATIC IMPORTED)
            set_target_properties(pacs::client PROPERTIES
                IMPORTED_LOCATION "${PACS_CLIENT_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${PACS_INCLUDE_DIRS}"
            )
            message(STATUS "  - pacs_client: ${PACS_CLIENT_LIB}")
        endif()

        if(PACS_WORKFLOW_LIB)
            add_library(pacs::workflow STATIC IMPORTED)
            set_target_properties(pacs::workflow PROPERTIES
                IMPORTED_LOCATION "${PACS_WORKFLOW_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${PACS_INCLUDE_DIRS}"
            )
            message(STATUS "  - pacs_workflow: ${PACS_WORKFLOW_LIB}")
        endif()

        if(PACS_MONITORING_LIB)
            add_library(pacs::monitoring STATIC IMPORTED)
            set_target_properties(pacs::monitoring PROPERTIES
                IMPORTED_LOCATION "${PACS_MONITORING_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${PACS_INCLUDE_DIRS}"
            )
            if(MONITORING_SYSTEM_LIB)
                set_property(TARGET pacs::monitoring APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES "${MONITORING_SYSTEM_LIB}")
            endif()
            message(STATUS "  - pacs_monitoring: ${PACS_MONITORING_LIB}")
        endif()

        if(PACS_AI_LIB)
            add_library(pacs::ai STATIC IMPORTED)
            set_target_properties(pacs::ai PROPERTIES
                IMPORTED_LOCATION "${PACS_AI_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${PACS_INCLUDE_DIRS}"
            )
            message(STATUS "  - pacs_ai: ${PACS_AI_LIB}")
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

        if(THREAD_BASE_LIB)
            add_library(pacs::thread_base STATIC IMPORTED)
            set_target_properties(pacs::thread_base PROPERTIES
                IMPORTED_LOCATION "${THREAD_BASE_LIB}"
            )
            message(STATUS "  - thread_base: ${THREAD_BASE_LIB}")
        endif()

        if(THREAD_CORE_LIB)
            add_library(pacs::thread_core STATIC IMPORTED)
            set_target_properties(pacs::thread_core PROPERTIES
                IMPORTED_LOCATION "${THREAD_CORE_LIB}"
            )
            message(STATUS "  - thread_core: ${THREAD_CORE_LIB}")
        endif()

        # Define macro to enable pacs_system code paths
        add_compile_definitions(DICOM_VIEWER_USE_PACS_SYSTEM)

        # Detect AddressSanitizer in pacs_system build and handle unresolved symbols
        if(EXISTS "${PACS_SYSTEM_BUILD_DIR}/CMakeCache.txt")
            file(READ "${PACS_SYSTEM_BUILD_DIR}/CMakeCache.txt" _pacs_cache_content)
            if(_pacs_cache_content MATCHES "-fsanitize=address")
                set(PACS_SYSTEM_HAS_ASAN TRUE)
                message(WARNING
                    "pacs_system (${PACS_SYSTEM_BUILD_DIR}) was built with AddressSanitizer.\n"
                    "  Using -undefined dynamic_lookup to suppress linker errors.\n"
                    "  To avoid this, rebuild pacs_system without ASan or set\n"
                    "  PACS_SYSTEM_BUILD_DIR to a clean build directory.")
                # Use dynamic_lookup to suppress unresolved ASan symbol errors.
                # Unlike -fsanitize=address, this does NOT load the ASan runtime,
                # so binaries won't deadlock from partial instrumentation.
                foreach(_pacs_tgt pacs::core pacs::network pacs::services)
                    set_property(TARGET ${_pacs_tgt} APPEND PROPERTY
                        INTERFACE_LINK_OPTIONS "-Wl,-undefined,dynamic_lookup")
                endforeach()
                foreach(_pacs_tgt pacs::encoding pacs::integration
                        pacs::security pacs::storage pacs::web
                        pacs::client pacs::workflow pacs::monitoring pacs::ai
                        pacs::container_system pacs::network_system
                        pacs::logger_system pacs::thread_base pacs::thread_core)
                    if(TARGET ${_pacs_tgt})
                        set_property(TARGET ${_pacs_tgt} APPEND PROPERTY
                            INTERFACE_LINK_OPTIONS "-Wl,-undefined,dynamic_lookup")
                    endif()
                endforeach()
                # Defer test discovery to ctest time so that build succeeds
                # even when pacs-linked binaries crash during --gtest_list_tests.
                set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)
            endif()
        endif()
    else()
        set(PACS_SYSTEM_FOUND FALSE)
        message(FATAL_ERROR "pacs_system libraries not found. Please build pacs_system first.")
    endif()
else()
    set(PACS_SYSTEM_FOUND FALSE)
    message(FATAL_ERROR "pacs_system not found at ${PACS_SYSTEM_ROOT}. Please set PACS_SYSTEM_ROOT to the pacs_system source directory.")
endif()

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(jwt-cpp CONFIG QUIET)

# Optional: libldap for LdapAuthProvider (LDAPS/AD authentication)
# On macOS, Apple's LDAP.framework has a broken self-referential header;
# only detect Homebrew OpenLDAP (or system OpenLDAP on Linux).
if(APPLE)
    # Apple's LDAP.framework has a broken self-referential header; require Homebrew OpenLDAP.
    # CMake may still find LDAP.framework via framework search paths — exclude frameworks explicitly.
    find_library(LIBLDAP_LIBRARY NAMES ldap
        HINTS /opt/homebrew/lib /opt/homebrew/opt/openldap/lib /usr/local/lib
        NO_DEFAULT_PATH
        NO_CMAKE_SYSTEM_FRAMEWORK_PATH
        NO_CMAKE_SYSTEM_PATH
    )
    find_path(LIBLDAP_INCLUDE_DIR ldap.h
        HINTS /opt/homebrew/include /opt/homebrew/opt/openldap/include /usr/local/include
        NO_DEFAULT_PATH
        NO_CMAKE_SYSTEM_FRAMEWORK_PATH
        NO_CMAKE_SYSTEM_PATH
    )
    # Reject Apple LDAP.framework even if somehow found above
    if(LIBLDAP_LIBRARY MATCHES "LDAP\\.framework")
        unset(LIBLDAP_LIBRARY CACHE)
        unset(LIBLDAP_INCLUDE_DIR CACHE)
    endif()
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBLDAP QUIET ldap)
    endif()
    if(NOT LIBLDAP_FOUND)
        find_library(LIBLDAP_LIBRARY NAMES ldap ldap_r)
        find_path(LIBLDAP_INCLUDE_DIR ldap.h)
    endif()
endif()

if(LIBLDAP_LIBRARY AND LIBLDAP_INCLUDE_DIR AND NOT LIBLDAP_FOUND)
    set(LIBLDAP_FOUND TRUE)
    set(LIBLDAP_LIBRARIES "${LIBLDAP_LIBRARY}")
    set(LIBLDAP_INCLUDE_DIRS "${LIBLDAP_INCLUDE_DIR}")
endif()

if(LIBLDAP_FOUND)
    message(STATUS "libldap found: LDAP authentication enabled (${LIBLDAP_LIBRARIES})")
else()
    message(STATUS "libldap not found: LdapAuthProvider compiled without LDAP support (configure-only mode)")
endif()

if(NOT TARGET jwt-cpp::jwt-cpp)
    include(FetchContent)
    FetchContent_Declare(
        jwt_cpp
        GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
        GIT_TAG        v0.7.1
        GIT_SHALLOW    TRUE
    )
    set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(JWT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(JWT_DISABLE_PICOJSON OFF CACHE BOOL "" FORCE)
    set(JWT_EXTERNAL_PICOJSON OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(jwt_cpp)
endif()

# Include directories (VTK/ITK headers propagated via imported targets in target_link_libraries)
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# Find FFTW for ITK's FFT operations (required by N4 bias correction and other filters)
find_library(FFTW3_LIBRARY fftw3)
find_library(FFTW3_THREADS_LIBRARY fftw3_threads)

# Core library
find_package(ZLIB REQUIRED)

add_library(dicom_viewer_core STATIC
    src/core/dicom/dicom_loader.cpp
    src/core/dicom/series_builder.cpp
    src/core/dicom/transfer_syntax_decoder.cpp
    src/core/image/image_converter.cpp
    src/core/image/hounsfield_converter.cpp
    src/core/zip_archive.cpp
    src/core/project_manager.cpp
    src/core/data_serializer.cpp
)

target_include_directories(dicom_viewer_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include/core
    ${COMMON_SYSTEM_INCLUDE_DIR}
    ${LOGGER_SYSTEM_INCLUDE_DIR}
    ${THREAD_SYSTEM_INCLUDE_DIR}
)

target_link_libraries(dicom_viewer_core PUBLIC
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
    ${FFTW3_LIBRARY}
    ${FFTW3_THREADS_LIBRARY}
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
)

# Authentication service (AuthProvider interface, LDAP and OIDC backends, factory)
add_library(auth_service STATIC
    src/services/auth/ldap_auth_provider.cpp
    src/services/auth/oidc_auth_provider.cpp
    src/services/auth/auth_provider_factory.cpp
    src/services/auth/rbac_middleware.cpp
)

target_include_directories(auth_service PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(auth_service PUBLIC
    jwt-cpp::jwt-cpp
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

if(LIBLDAP_FOUND)
    target_compile_definitions(auth_service PUBLIC DICOM_VIEWER_HAS_LDAP)
    target_include_directories(auth_service PRIVATE ${LIBLDAP_INCLUDE_DIRS})
    target_link_libraries(auth_service PUBLIC ${LIBLDAP_LIBRARIES})
endif()

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
    src/services/render/hemodynamic_overlay_renderer.cpp
    src/services/render/streamline_overlay_renderer.cpp
    src/services/render/hemodynamic_surface_manager.cpp
    src/services/render/asc_view_controller.cpp
    src/services/render/offscreen_render_context.cpp
    src/services/render/render_session.cpp
    src/services/render/frame_encoder.cpp
    src/services/render/websocket_frame_streamer.cpp
    src/services/render/input_event_dispatcher.cpp
    src/services/render/render_session_manager.cpp
    src/services/render/adaptive_quality_controller.cpp
    src/services/render/dirty_region_tracker.cpp
    src/services/render/session_token_validator.cpp
    src/services/render/gpu_memory_budget_manager.cpp
)

# Crow WebSocket framework (header-only)
# Try pacs_system build directory first, then fall back to FetchContent
set(CROW_INCLUDE_DIR "${PACS_SYSTEM_BUILD_DIR}/_deps/crow-src/include")
set(ASIO_INCLUDE_DIR "${PACS_SYSTEM_BUILD_DIR}/_deps/asio-src/asio/include")

target_link_libraries(render_service PUBLIC
    dicom_viewer_core
    coordinate_service
    store_service
    ${VTK_LIBRARIES}
    jwt-cpp::jwt-cpp
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CMAKE_DL_LIBS}
)

if(EXISTS "${CROW_INCLUDE_DIR}/crow.h" AND EXISTS "${ASIO_INCLUDE_DIR}/asio.hpp")
    target_include_directories(render_service PUBLIC
        ${CROW_INCLUDE_DIR}
        ${ASIO_INCLUDE_DIR}
    )
    target_compile_definitions(render_service PRIVATE CROW_DISABLE_STATIC_DIR)
    message(STATUS "  Crow WebSocket: enabled (from pacs_system: ${CROW_INCLUDE_DIR})")
else()
    message(STATUS "  Crow WebSocket: not found in pacs_system, fetching via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        asio_standalone
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG        asio-1-30-2
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(
        Crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CROW_ENABLE_SSL OFF CACHE BOOL "" FORCE)
    set(CROW_ENABLE_COMPRESSION OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(asio_standalone Crow)
    target_include_directories(render_service PUBLIC
        ${asio_standalone_SOURCE_DIR}/asio/include
        ${crow_SOURCE_DIR}/include
    )
    target_compile_definitions(render_service PRIVATE CROW_DISABLE_STATIC_DIR)
    message(STATUS "  Crow WebSocket: enabled (via FetchContent)")
endif()

add_library(measurement_service STATIC
    src/services/measurement/linear_measurement_tool.cpp
    src/services/measurement/area_measurement_tool.cpp
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
    src/services/segmentation/segmentation_command.cpp
    src/services/segmentation/brush_stroke_command.cpp
    src/services/segmentation/mask_boolean_operations.cpp
    src/services/segmentation/snapshot_command.cpp
    src/services/segmentation/phase_tracker.cpp
    src/services/segmentation/hollow_tool.cpp
    src/services/segmentation/mask_smoother.cpp
    src/services/segmentation/centerline_tracer.cpp
    src/services/segmentation/level_tracing_tool.cpp
)

target_link_libraries(segmentation_service PUBLIC
    dicom_viewer_core
    coordinate_service
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Deployment configuration loader
add_library(config_service STATIC
    src/services/config/deployment_config.cpp
)

target_include_directories(config_service PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(config_service PUBLIC
    spdlog::spdlog
)

# Session/audit store service (Redis, PostgreSQL)
find_package(hiredis CONFIG QUIET)
find_package(PostgreSQL QUIET)

set(STORE_SERVICE_SOURCES
    src/services/store/in_memory_session_store.cpp
)

if(hiredis_FOUND)
    list(APPEND STORE_SERVICE_SOURCES src/services/store/redis_session_store.cpp)
endif()

if(PostgreSQL_FOUND)
    list(APPEND STORE_SERVICE_SOURCES src/services/store/postgres_audit_sink.cpp)
endif()

add_library(store_service STATIC ${STORE_SERVICE_SOURCES})

target_include_directories(store_service PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(store_service PUBLIC
    spdlog::spdlog
)

if(hiredis_FOUND)
    target_link_libraries(store_service PUBLIC hiredis::hiredis)
    target_compile_definitions(store_service PUBLIC DICOM_VIEWER_HAS_HIREDIS=1)
    message(STATUS "  hiredis: found (Redis session store enabled)")
else()
    message(STATUS "  hiredis: not found (Redis session store disabled)")
endif()

if(PostgreSQL_FOUND)
    target_include_directories(store_service PRIVATE ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(store_service PUBLIC PostgreSQL::PostgreSQL)
    target_compile_definitions(store_service PUBLIC DICOM_VIEWER_HAS_LIBPQ=1)
    message(STATUS "  libpq: found (PostgreSQL audit sink enabled)")
else()
    message(STATUS "  libpq: not found (PostgreSQL audit sink disabled)")
endif()

add_library(pacs_service STATIC
    src/services/pacs/dicom_echo_scu.cpp
    src/services/pacs/dicom_find_scu.cpp
    src/services/pacs/dicom_move_scu.cpp
    src/services/pacs/dicom_store_scp.cpp
    src/services/pacs/pacs_config_manager.cpp
    src/services/pacs/storage_commitment_service.cpp
    src/services/pacs/audit_service.cpp
    src/services/pacs/query_cache_manager.cpp
    src/services/pacs/dicomweb_service.cpp
    include/services/pacs_config_manager.hpp
    include/services/dicom_store_scp.hpp
    include/services/storage_commitment_service.hpp
    include/services/audit_service.hpp
    include/services/query_cache_manager.hpp
    include/services/dicomweb_service.hpp
)

target_include_directories(pacs_service PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${PACS_SYSTEM_INCLUDE_DIR}
)

target_link_libraries(pacs_service PUBLIC
    pacs::core
    pacs::network
    pacs::services
    spdlog::spdlog
    fmt::fmt
)

target_link_libraries(pacs_service PUBLIC nlohmann_json::nlohmann_json)

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

if(TARGET pacs::thread_base)
    target_link_libraries(pacs_service PUBLIC pacs::thread_base)
endif()

if(TARGET pacs::thread_core)
    target_link_libraries(pacs_service PUBLIC pacs::thread_core)
endif()

# pacs_services depends on pacs_security (ATNA audit in storage_scp)
if(TARGET pacs::security)
    target_link_libraries(pacs_service PUBLIC pacs::security)
endif()

# pacs_web provides DICOMweb REST API (WADO-RS, STOW-RS, QIDO-RS)
if(TARGET pacs::web)
    target_link_libraries(pacs_service PUBLIC pacs::web)
endif()

# pacs_web transitive dependencies
if(TARGET pacs::storage)
    target_link_libraries(pacs_service PUBLIC pacs::storage)
endif()

if(TARGET pacs::client)
    target_link_libraries(pacs_service PUBLIC pacs::client)
endif()

if(TARGET pacs::monitoring)
    target_link_libraries(pacs_service PUBLIC pacs::monitoring)
endif()

add_library(export_service STATIC
    src/services/export/report_generator.cpp
    src/services/export/data_exporter.cpp
    src/services/export/measurement_serializer.cpp
    src/services/export/mesh_exporter.cpp
    src/services/export/dicom_sr_writer.cpp
    src/services/export/ensight_exporter.cpp
    src/services/export/matlab_exporter.cpp
    src/services/export/video_exporter.cpp
)

target_include_directories(export_service PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${PACS_SYSTEM_INCLUDE_DIR}
)

target_link_libraries(export_service PUBLIC
    dicom_viewer_core
    measurement_service
    segmentation_service
    pacs_service
    nlohmann_json::nlohmann_json
    ${VTK_LIBRARIES}
)

# Flow analysis service (4D Flow MRI)
add_library(flow_service STATIC
    src/services/flow/flow_dicom_parser.cpp
    src/services/flow/velocity_field_assembler.cpp
    src/services/flow/phase_corrector.cpp
    src/services/flow/temporal_navigator.cpp
    src/services/flow/flow_visualizer.cpp
    src/services/flow/flow_quantifier.cpp
    src/services/flow/vessel_analyzer.cpp
    src/services/flow/vendor_parsers/siemens_flow_parser.cpp
    src/services/flow/vendor_parsers/philips_flow_parser.cpp
    src/services/flow/vendor_parsers/ge_flow_parser.cpp
)

target_link_libraries(flow_service PUBLIC
    dicom_viewer_core
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
)

# Enhanced DICOM service (multi-frame IOD parsing)
add_library(enhanced_dicom_service STATIC
    src/services/enhanced_dicom/enhanced_dicom_parser.cpp
    src/services/enhanced_dicom/functional_group_parser.cpp
    src/services/enhanced_dicom/frame_extractor.cpp
    src/services/enhanced_dicom/dimension_index_sorter.cpp
    src/services/enhanced_dicom/series_classifier.cpp
)

target_link_libraries(enhanced_dicom_service PUBLIC
    dicom_viewer_core
    ${ITK_LIBRARIES}
    ${GDCM_LIBRARIES}
)

# Cardiac analysis service (ECG-gated phase detection, calcium scoring, coronary CTA, cine MRI)
add_library(cardiac_service STATIC
    src/services/cardiac/cardiac_phase_detector.cpp
    src/services/cardiac/calcium_scorer.cpp
    src/services/cardiac/coronary_centerline_extractor.cpp
    src/services/cardiac/curved_planar_reformatter.cpp
    src/services/cardiac/cine_organizer.cpp
)

target_link_libraries(cardiac_service PUBLIC
    dicom_viewer_core
    enhanced_dicom_service
    flow_service
    ${ITK_LIBRARIES}
    ${VTK_LIBRARIES}
)

# Server build — delegate to server/CMakeLists.txt
add_subdirectory(server)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Build configuration summary
message(STATUS "")
message(STATUS "=== dicom_viewer Build Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build mode: server-only")
message(STATUS "")
message(STATUS "DICOM Network Library:")
message(STATUS "  pacs_system: Found and enabled")
message(STATUS "    Root: ${PACS_SYSTEM_ROOT}")
message(STATUS "    Build: ${PACS_SYSTEM_BUILD_DIR}")
message(STATUS "")
