#!/usr/bin/env bash
#
# setup.sh - Bootstrap script for dicom_viewer development environment
#
# Usage:
#   ./setup.sh                  # Full setup (dependencies + ecosystem + build)
#   ./setup.sh --skip-deps      # Skip system dependency installation
#   ./setup.sh --skip-ecosystem # Skip ecosystem repository cloning/building
#   ./setup.sh --help           # Show usage
#

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARENT_DIR="$(dirname "${PROJECT_DIR}")"
SKIP_DEPS=false
SKIP_ECOSYSTEM=false
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

# Ecosystem repositories (clone order matters for build dependencies)
ECOSYSTEM_REPOS=(
    common_system
    container_system
    thread_system
    logger_system
    network_system
    pacs_system
)
ECOSYSTEM_ORG="kcenon"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Bootstrap the dicom_viewer development environment from a fresh clone."
    echo ""
    echo "Options:"
    echo "  --skip-deps       Skip system dependency installation"
    echo "  --skip-ecosystem  Skip ecosystem repository cloning and building"
    echo "  --jobs N          Set parallel build jobs (default: ${JOBS})"
    echo "  --help            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                        # Full setup (first time)"
    echo "  $0 --skip-deps            # Only ecosystem clone + build"
    echo "  $0 --skip-ecosystem       # Only system dependencies"
    echo ""
    echo "After setup completes, build with:"
    echo "  ./build.sh --test"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-deps)
            SKIP_DEPS=true
            shift
            ;;
        --skip-ecosystem)
            SKIP_ECOSYSTEM=true
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

# --- Platform detection ---

detect_platform() {
    OS="$(uname -s)"
    ARCH="$(uname -m)"

    case "${OS}" in
        Darwin)
            PLATFORM="macos"
            if [[ "${ARCH}" == "arm64" ]]; then
                PLATFORM_DESC="macOS (Apple Silicon)"
            else
                PLATFORM_DESC="macOS (Intel)"
            fi
            ;;
        Linux)
            PLATFORM="linux"
            PLATFORM_DESC="Linux (${ARCH})"
            ;;
        *)
            echo "Error: Unsupported platform '${OS}'"
            exit 1
            ;;
    esac
}

detect_platform

echo "=== dicom_viewer setup ==="
echo "  Platform: ${PLATFORM_DESC}"
echo "  Jobs:     ${JOBS}"
echo ""

# --- Prerequisite checks ---

echo "--- Checking prerequisites ---"

check_command() {
    if ! command -v "$1" &>/dev/null; then
        echo "Error: $1 is not installed. $2"
        exit 1
    fi
}

check_command git "Install from https://git-scm.com"
check_command cmake "Install CMake 3.20+: https://cmake.org/download/"

if [[ "${PLATFORM}" == "macos" ]]; then
    check_command brew "Install Homebrew: https://brew.sh"
fi

echo "  git:   $(git --version | cut -d' ' -f3)"
echo "  cmake: $(cmake --version | head -1 | cut -d' ' -f3)"
if [[ "${PLATFORM}" == "macos" ]]; then
    echo "  brew:  $(brew --version | head -1 | cut -d' ' -f2)"
fi
echo ""

# --- Step 1: System dependencies ---

BREW_PACKAGES=(
    qt@6
    vtk
    itk
    gdcm
    fftw
    spdlog
    fmt
    nlohmann-json
    googletest
    expat
)

APT_PACKAGES=(
    qt6-base-dev
    libvtk9-dev
    libinsighttoolkit5-dev
    libgdcm-dev
    libfftw3-dev
    libspdlog-dev
    libfmt-dev
    nlohmann-json3-dev
    libgtest-dev
    libexpat1-dev
)

install_dependencies() {
    echo "--- Installing system dependencies ---"

    if [[ "${PLATFORM}" == "macos" ]]; then
        local to_install=()
        for pkg in "${BREW_PACKAGES[@]}"; do
            if brew list "${pkg}" &>/dev/null; then
                echo "  [skip] ${pkg} (already installed)"
            else
                to_install+=("${pkg}")
            fi
        done

        if [[ ${#to_install[@]} -gt 0 ]]; then
            echo ""
            echo "  Installing: ${to_install[*]}"
            brew install "${to_install[@]}"
        fi
    elif [[ "${PLATFORM}" == "linux" ]]; then
        echo "  Updating package list..."
        sudo apt-get update -qq

        local to_install=()
        for pkg in "${APT_PACKAGES[@]}"; do
            if dpkg -s "${pkg}" &>/dev/null 2>&1; then
                echo "  [skip] ${pkg} (already installed)"
            else
                to_install+=("${pkg}")
            fi
        done

        if [[ ${#to_install[@]} -gt 0 ]]; then
            echo ""
            echo "  Installing: ${to_install[*]}"
            sudo apt-get install -y "${to_install[@]}"
        fi
    fi

    echo ""
    echo "  System dependencies ready."
    echo ""
}

if [[ "${SKIP_DEPS}" == false ]]; then
    install_dependencies
else
    echo "--- Skipping system dependencies (--skip-deps) ---"
    echo ""
fi

# --- Step 2: Ecosystem repositories ---

clone_ecosystem() {
    echo "--- Cloning ecosystem repositories ---"

    for repo in "${ECOSYSTEM_REPOS[@]}"; do
        local repo_path="${PARENT_DIR}/${repo}"
        if [[ -d "${repo_path}" ]]; then
            echo "  [skip] ${repo} (already exists at ${repo_path})"
        else
            echo "  Cloning ${repo}..."
            git clone --depth 1 "https://github.com/${ECOSYSTEM_ORG}/${repo}.git" "${repo_path}"
        fi
    done

    echo ""
    echo "  Ecosystem repositories ready."
    echo ""
}

build_pacs_system() {
    echo "--- Building pacs_system ---"

    local pacs_dir="${PARENT_DIR}/pacs_system"
    local pacs_build="${pacs_dir}/build"

    if [[ ! -d "${pacs_dir}" ]]; then
        echo "Error: pacs_system not found at ${pacs_dir}"
        echo "Hint: Run without --skip-ecosystem to clone it first."
        exit 1
    fi

    # Skip rebuild if libraries already exist
    if [[ -f "${pacs_build}/lib/libpacs_core.a" ]]; then
        echo "  [skip] pacs_system already built (${pacs_build}/lib/libpacs_core.a exists)"
        echo ""
        return
    fi

    echo "  Configuring..."
    cmake -S "${pacs_dir}" -B "${pacs_build}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DPACS_BUILD_TESTS=OFF \
        -DPACS_BUILD_EXAMPLES=OFF \
        -DPACS_BUILD_BENCHMARKS=OFF \
        -DPACS_BUILD_SAMPLES=OFF \
        -DPACS_WARNINGS_AS_ERRORS=OFF

    echo "  Building (jobs: ${JOBS})..."
    cmake --build "${pacs_build}" --config Release -j "${JOBS}"

    # Verify build output
    if [[ ! -f "${pacs_build}/lib/libpacs_core.a" ]]; then
        echo ""
        echo "Error: pacs_system build completed but libpacs_core.a not found."
        echo "Hint: Check the build output above for errors."
        exit 1
    fi

    echo ""
    echo "  pacs_system build complete."
    echo ""
}

if [[ "${SKIP_ECOSYSTEM}" == false ]]; then
    clone_ecosystem
    build_pacs_system
else
    echo "--- Skipping ecosystem setup (--skip-ecosystem) ---"
    echo ""
fi

# --- Step 3: Validation ---

echo "--- Validating setup ---"

VALIDATION_PASSED=true

validate_dir() {
    if [[ -d "$1" ]]; then
        echo "  [ok]   $2"
    else
        echo "  [FAIL] $2 (not found: $1)"
        VALIDATION_PASSED=false
    fi
}

validate_file() {
    if [[ -f "$1" ]]; then
        echo "  [ok]   $2"
    else
        echo "  [FAIL] $2 (not found: $1)"
        VALIDATION_PASSED=false
    fi
}

# Validate ecosystem repos
if [[ "${SKIP_ECOSYSTEM}" == false ]]; then
    for repo in "${ECOSYSTEM_REPOS[@]}"; do
        validate_dir "${PARENT_DIR}/${repo}" "${repo}"
    done
    validate_file "${PARENT_DIR}/pacs_system/build/lib/libpacs_core.a" "pacs_system libraries"
fi

# Validate key system packages (macOS only - check Homebrew cellar)
if [[ "${SKIP_DEPS}" == false && "${PLATFORM}" == "macos" ]]; then
    for pkg in "${BREW_PACKAGES[@]}"; do
        if brew list "${pkg}" &>/dev/null; then
            echo "  [ok]   ${pkg}"
        else
            echo "  [FAIL] ${pkg} (not installed)"
            VALIDATION_PASSED=false
        fi
    done
fi

echo ""

if [[ "${VALIDATION_PASSED}" == false ]]; then
    echo "=== Setup completed with errors ==="
    echo "Some validation checks failed. Review the output above."
    exit 1
fi

echo "=== Setup complete ==="
echo ""
echo "Next steps:"
echo "  ./build.sh --test    # Build and run tests"
