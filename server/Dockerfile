# Multi-stage Dockerfile for dicom_viewer
# Requires NVIDIA Container Toolkit for GPU rendering via EGL
#
# Build: docker build -f server/Dockerfile -t dicom-viewer-server .
# Run:   docker run --gpus all -p 8080:8080 -p 8081:8081 dicom-viewer-server

# =============================================================================
# Stage 1: Builder — compile all C++ dependencies and server binary
# =============================================================================
FROM nvidia/cuda:12.4.1-devel-ubuntu22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV VCPKG_FORCE_SYSTEM_BINARIES=1

# Build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git curl zip unzip tar pkg-config \
    python3 python3-pip \
    libegl1-mesa-dev libgles2-mesa-dev libgl1-mesa-dev \
    libx11-dev libxt-dev \
    libssl-dev libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
WORKDIR /opt
RUN git clone --depth 1 https://github.com/microsoft/vcpkg.git \
    && ./vcpkg/bootstrap-vcpkg.sh -disableMetrics

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

# Copy source
WORKDIR /src
COPY . .

# Build VTK with EGL support (headless rendering)
# VTK is built from source to enable EGL backend
RUN mkdir -p /build/vtk && cd /build/vtk \
    && git clone --depth 1 --branch v9.3.1 https://gitlab.kitware.com/vtk/vtk.git src \
    && cmake -S src -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/vtk \
        -DVTK_USE_X=OFF \
        -DVTK_OPENGL_HAS_EGL=ON \
        -DVTK_BUILD_TESTING=OFF \
        -DVTK_BUILD_EXAMPLES=OFF \
        -DVTK_GROUP_ENABLE_Rendering=YES \
        -DVTK_GROUP_ENABLE_StandAlone=YES \
        -DVTK_GROUP_ENABLE_Web=NO \
        -DVTK_GROUP_ENABLE_Qt=NO \
    && cmake --build build -j "$(nproc)" \
    && cmake --install build

# Build ITK
RUN mkdir -p /build/itk && cd /build/itk \
    && git clone --depth 1 --branch v5.4.0 https://github.com/InsightSoftwareConsortium/ITK.git src \
    && cmake -S src -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/itk \
        -DBUILD_TESTING=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DModule_ITKGDCM=ON \
    && cmake --build build -j "$(nproc)" \
    && cmake --install build

# Build dicom_viewer
RUN mkdir -p /build/server && cd /build/server \
    && cmake -S /src -B . -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=OFF \
        -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
        -DVTK_DIR=/opt/vtk/lib/cmake/vtk-9.3 \
        -DITK_DIR=/opt/itk/lib/cmake/ITK-5.4 \
    && cmake --build . -j "$(nproc)"

# =============================================================================
# Stage 2: Runtime — minimal image with server binary and GPU libs
# =============================================================================
FROM nvidia/cuda:12.4.1-runtime-ubuntu22.04

ENV DEBIAN_FRONTEND=noninteractive

# Runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libegl1-mesa libnvidia-gl-535 \
    libssl3 libcurl4 \
    wkhtmltopdf \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy built artifacts
COPY --from=builder /opt/vtk/lib /opt/vtk/lib
COPY --from=builder /opt/itk/lib /opt/itk/lib
COPY --from=builder /build/server/bin/dicom_viewer /usr/local/bin/
COPY --from=builder /build/server/lib /usr/local/lib/

# Library paths
ENV LD_LIBRARY_PATH="/opt/vtk/lib:/opt/itk/lib:/usr/local/lib:${LD_LIBRARY_PATH}"

# Headless rendering
ENV VTK_DEFAULT_RENDER_WINDOW_HEADLESS=1
ENV NVIDIA_VISIBLE_DEVICES=all
ENV NVIDIA_DRIVER_CAPABILITIES=compute,graphics,utility

EXPOSE 8080 8081

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8080/api/v1/health || exit 1

ENTRYPOINT ["dicom_viewer"]
CMD ["--port", "8080", "--ws-port", "8081"]
