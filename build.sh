#!/usr/bin/env bash
#
# build.sh - Build script for dicom_viewer
#
# Usage:
#   ./build.sh              # Configure + build (Release)
#   ./build.sh --debug      # Configure + build (Debug)
#   ./build.sh --clean      # Clean build directory and rebuild
#   ./build.sh --test       # Build and run tests
#   ./build.sh --help       # Show usage
#

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BUILD_TYPE="Release"
CLEAN=false
RUN_TESTS=false
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --debug       Build in Debug mode (default: Release)"
    echo "  --clean       Remove build directory and rebuild from scratch"
    echo "  --test        Run tests after building"
    echo "  --jobs N      Set parallel build jobs (default: ${JOBS})"
    echo "  --help        Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                    # Release build"
    echo "  $0 --debug --test    # Debug build + run tests"
    echo "  $0 --clean --test    # Clean rebuild + run tests"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --test)
            RUN_TESTS=true
            shift
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "Error: Unknown option '$1'"
            usage
            exit 1
            ;;
    esac
done

echo "=== dicom_viewer build ==="
echo "  Build type: ${BUILD_TYPE}"
echo "  Build dir:  ${BUILD_DIR}"
echo "  Jobs:       ${JOBS}"
echo ""

# Clean if requested
if [[ "${CLEAN}" == true ]]; then
    echo "--- Cleaning build directory ---"
    rm -rf "${BUILD_DIR}"
fi

# Configure
if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    echo "--- Configuring (CMake) ---"
    cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    echo ""
fi

# Build
echo "--- Building ---"
cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" -j "${JOBS}"
echo ""

echo "=== Build complete ==="

# Run tests
if [[ "${RUN_TESTS}" == true ]]; then
    echo ""
    echo "--- Running tests ---"
    ctest --test-dir "${BUILD_DIR}" --output-on-failure -C "${BUILD_TYPE}" || {
        echo ""
        echo "=== Some tests failed ==="
        exit 1
    }
    echo ""
    echo "=== All tests passed ==="
fi
