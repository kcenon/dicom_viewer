#!/usr/bin/env bash
#
# build.sh - Build script for dicom_viewer
#
# Usage:
#   ./build.sh              # Configure + build (Release)
#   ./build.sh --debug      # Configure + build (Debug)
#   ./build.sh --clean      # Clean build directory and rebuild
#   ./build.sh --test       # Build and run tests
#   ./build.sh --skip-checks # Skip preflight dependency validation
#   ./build.sh --help       # Show usage
#

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BUILD_TYPE="Release"
CLEAN=false
RUN_TESTS=false
SKIP_CHECKS=false
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --debug       Build in Debug mode (default: Release)"
    echo "  --clean       Remove build directory and rebuild from scratch"
    echo "  --test        Run tests after building"
    echo "  --skip-checks Skip preflight dependency validation"
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
        --skip-checks)
            SKIP_CHECKS=true
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

# --- Preflight dependency validation ---

preflight_check() {
    local errors=0

    echo "--- Preflight checks ---"

    # 1. CMake version >= 3.20
    if command -v cmake &>/dev/null; then
        local cmake_ver
        cmake_ver=$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
        local cmake_major="${cmake_ver%%.*}"
        local cmake_minor="${cmake_ver#*.}"
        if [[ "${cmake_major}" -lt 3 ]] || { [[ "${cmake_major}" -eq 3 ]] && [[ "${cmake_minor}" -lt 20 ]]; }; then
            echo "  [FAIL] CMake ${cmake_ver} found, but 3.20+ required"
            echo "         Upgrade: https://cmake.org/download/"
            errors=$((errors + 1))
        else
            echo "  [ok]   CMake ${cmake_ver}"
        fi
    else
        echo "  [FAIL] CMake not found"
        echo "         Install: https://cmake.org/download/"
        errors=$((errors + 1))
    fi

    # 2. pacs_system build
    local pacs_build="${PROJECT_DIR}/../pacs_system/build"
    if [[ -f "${pacs_build}/lib/libpacs_core.a" ]]; then
        echo "  [ok]   pacs_system libraries"
    else
        echo "  [FAIL] pacs_system not built at ${pacs_build}"
        echo "         Run: ./setup.sh"
        errors=$((errors + 1))
    fi

    # 3. macOS: Xcode Command Line Tools
    if [[ "$(uname -s)" == "Darwin" ]]; then
        if xcode-select -p &>/dev/null; then
            echo "  [ok]   Xcode Command Line Tools"
        else
            echo "  [FAIL] Xcode Command Line Tools not installed"
            echo "         Run: xcode-select --install"
            errors=$((errors + 1))
        fi
    fi

    # 4. Optional package warnings (non-blocking)
    if [[ "$(uname -s)" == "Darwin" ]] && command -v brew &>/dev/null; then
        for pkg in qt@6 vtk itk; do
            if ! brew list "${pkg}" &>/dev/null; then
                echo "  [warn] ${pkg} not found via Homebrew"
                echo "         Install: brew install ${pkg}"
            fi
        done
    fi

    echo ""

    if [[ ${errors} -gt 0 ]]; then
        echo "Error: Preflight checks failed (${errors} error(s))."
        echo "  Run ./setup.sh to install dependencies, or use --skip-checks to bypass."
        exit 1
    fi
}

if [[ "${SKIP_CHECKS}" == false ]]; then
    preflight_check
fi

# Clean if requested
if [[ "${CLEAN}" == true ]]; then
    echo "--- Cleaning build directory ---"
    rm -rf "${BUILD_DIR}"
fi

# Platform-specific prefix path for dependency discovery
CMAKE_PREFIX_PATH_ARG=""
if [[ -z "${CMAKE_PREFIX_PATH:-}" ]]; then
    if [[ "$(uname)" == "Darwin" ]]; then
        if [[ "$(uname -m)" == "arm64" ]]; then
            CMAKE_PREFIX_PATH_ARG="-DCMAKE_PREFIX_PATH=/opt/homebrew"
        else
            CMAKE_PREFIX_PATH_ARG="-DCMAKE_PREFIX_PATH=/usr/local"
        fi
    fi
fi

# Configure
if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    echo "--- Configuring (CMake) ---"
    cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        ${CMAKE_PREFIX_PATH_ARG} || {
        echo ""
        echo "CMake configuration failed. Common causes:"
        echo "  - Missing dependencies: run ./setup.sh"
        echo "  - Stale cache: run ./build.sh --clean"
        echo "  - See README.md for prerequisites"
        exit 1
    }
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
