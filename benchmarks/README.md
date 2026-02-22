# Benchmarks

Performance benchmarks for dicom_viewer.

## Purpose

This directory contains benchmark scripts and results for measuring dicom_viewer performance across its critical paths: DICOM loading, volume rendering, MPR slice switching, segmentation, and memory usage.

## Metrics

| Metric | Description | Target |
|--------|-------------|--------|
| **DICOM Load Time** | Time to scan directory and build 3D volume | <= 3 sec (512x512x300) |
| **Volume Rendering FPS** | Frames per second during interactive rotation | >= 30 FPS |
| **MPR Slice Switch** | Latency for single slice navigation | <= 100 ms |
| **Segmentation Throughput** | Time for threshold segmentation on full volume | Measured |
| **Memory Usage** | Peak RSS during volume load and rendering | <= 2 GB (1 GB volume) |
| **Application Startup** | Cold start to window display | <= 5 sec |

## Existing Test-Based Benchmarks

The following benchmark tests are built as part of the test suite in `tests/`:

```bash
# Performance benchmark (loading, segmentation, export)
ctest --test-dir build -R "performance_benchmark" --output-on-failure

# Rendering benchmark (volume rendering, MPR, surface rendering)
ctest --test-dir build -R "rendering_benchmark" --output-on-failure
```

## Running Benchmarks

### Prerequisites

- Completed build (`cmake --build build --config Release`)
- Test data in `tests/data/` (DICOM sample datasets)

### Run All Benchmarks

```bash
ctest --test-dir build -R "[Bb]enchmark" --output-on-failure
```

### Run Individual Benchmarks

```bash
# Performance benchmarks only
./build/bin/performance_benchmark_test

# Rendering benchmarks only
./build/bin/rendering_benchmark_test
```

## Output Format

Benchmark tests report timing data via Google Test output:

```
[ RUN      ] PerformanceBenchmark.VolumeLoadTime
             Volume size: 512 x 512 x 300
             Load time: 2.45 sec
[       OK ] PerformanceBenchmark.VolumeLoadTime (2450 ms)
```

## Adding New Benchmarks

1. Create a new test file in `tests/unit/` with the `_benchmark_test.cpp` suffix
2. Use the `BenchmarkFixture` from `tests/test_utils/benchmark_fixture.hpp`
3. Register with CTest via `gtest_discover_tests` in `tests/CMakeLists.txt`
4. Document the new metric in this README

## Results Tracking

Benchmark results are not committed to the repository. To track results over time:

1. Run benchmarks before and after changes
2. Compare output timing values
3. Record significant regressions in commit messages or PR descriptions

---

**Date**: 2026-02-23
