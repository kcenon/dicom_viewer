// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "services/dicom_echo_scu.hpp"
#include "services/flow/flow_quantifier.hpp"
#include "services/flow/temporal_navigator.hpp"
#include "services/preprocessing/gaussian_smoother.hpp"
#include "services/segmentation/label_manager.hpp"
#include "services/segmentation/manual_segmentation_controller.hpp"
#include "services/segmentation/morphological_processor.hpp"
#include "services/segmentation/region_growing_segmenter.hpp"

#include "../test_utils/flow_phantom_generator.hpp"
#include "../test_utils/volume_generator.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <latch>
#include <thread>
#include <vector>

namespace dicom_viewer::services {
namespace {

using test_utils::createBinaryMaskVolume;
using test_utils::createSyntheticCTVolume;
using test_utils::createVolume;

// =============================================================================
// PACS Concurrency Tests
// =============================================================================

class PacsConcurrencyTest : public ::testing::Test {
protected:
    PacsServerConfig makeUnreachableConfig() {
        PacsServerConfig config;
        // RFC 5737 TEST-NET-1: guaranteed non-routable address
        config.hostname = "192.0.2.1";
        config.port = 104;
        config.calledAeTitle = "TEST_SERVER";
        config.callingAeTitle = "TEST_CLIENT";
        config.connectionTimeout = std::chrono::seconds(1);
        config.dimseTimeout = std::chrono::seconds(1);
        return config;
    }
};

TEST_F(PacsConcurrencyTest, CancelDuringVerify) {
    DicomEchoSCU echo;
    auto config = makeUnreachableConfig();

    std::atomic<bool> verifyStarted{false};
    std::atomic<bool> verifyDone{false};

    std::thread verifyThread([&] {
        verifyStarted.store(true);
        [[maybe_unused]] auto result = echo.verify(config);
        verifyDone.store(true);
    });

    // Wait for verify to begin
    while (!verifyStarted.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Cancel from main thread (documented as thread-safe)
    echo.cancel();

    verifyThread.join();
    EXPECT_TRUE(verifyDone.load()) << "verify() should complete after cancel()";
}

TEST_F(PacsConcurrencyTest, ConcurrentVerifyOnSeparateInstances) {
    constexpr int kThreadCount = 4;
    auto config = makeUnreachableConfig();

    std::vector<DicomEchoSCU> echos(kThreadCount);
    std::latch startLatch(kThreadCount);
    std::atomic<int> completedCount{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i] {
            startLatch.arrive_and_wait();
            [[maybe_unused]] auto result = echos[i].verify(config);
            completedCount.fetch_add(1);
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(completedCount.load(), kThreadCount);
}

TEST_F(PacsConcurrencyTest, ConcurrentStateQueryDuringVerify) {
    DicomEchoSCU echo;
    auto config = makeUnreachableConfig();

    std::atomic<bool> stopQuerying{false};
    std::atomic<int> queryCount{0};

    std::thread verifyThread([&] {
        [[maybe_unused]] auto result = echo.verify(config);
        stopQuerying.store(true);
    });

    std::thread queryThread([&] {
        while (!stopQuerying.load()) {
            [[maybe_unused]] bool verifying = echo.isVerifying();
            queryCount.fetch_add(1);
            std::this_thread::yield();
        }
    });

    verifyThread.join();
    queryThread.join();

    EXPECT_GT(queryCount.load(), 0) << "Should have queried state at least once";
}

// =============================================================================
// Segmentation Concurrency Tests
// =============================================================================

class SegmentationConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto result = manager_.initializeLabelMap(64, 64, 64);
        ASSERT_TRUE(result.has_value());
    }

    LabelManager manager_;
};

TEST_F(SegmentationConcurrencyTest, LabelManagerConcurrentAdd) {
    constexpr int kThreadCount = 8;
    constexpr int kLabelsPerThread = 10;

    std::latch startLatch(kThreadCount);
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i] {
            startLatch.arrive_and_wait();
            for (int j = 0; j < kLabelsPerThread; ++j) {
                auto name = "label_" + std::to_string(i * 100 + j);
                auto result = manager_.addLabel(name);
                if (result.has_value()) {
                    successCount.fetch_add(1);
                } else {
                    errorCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    int total = successCount.load() + errorCount.load();
    EXPECT_EQ(total, kThreadCount * kLabelsPerThread);
    EXPECT_GT(successCount.load(), 0);

    // Label count must equal successful additions
    EXPECT_EQ(manager_.getLabelCount(),
              static_cast<size_t>(successCount.load()));
}

TEST_F(SegmentationConcurrencyTest, LabelManagerConcurrentAddAndRemove) {
    // Pre-populate labels
    for (int i = 0; i < 20; ++i) {
        auto result = manager_.addLabel("pre_" + std::to_string(i));
        ASSERT_TRUE(result.has_value());
    }

    std::latch startLatch(2);

    // Thread 1: Add labels
    std::thread adder([&] {
        startLatch.arrive_and_wait();
        for (int i = 0; i < 50; ++i) {
            [[maybe_unused]] auto r = manager_.addLabel(
                "add_" + std::to_string(i));
        }
    });

    // Thread 2: Remove labels (some may fail if already removed)
    std::thread remover([&] {
        startLatch.arrive_and_wait();
        for (uint8_t id = 1; id <= 30; ++id) {
            [[maybe_unused]] auto r = manager_.removeLabel(id, false);
        }
    });

    adder.join();
    remover.join();

    // Verify internal consistency: count matches actual label list
    size_t count = manager_.getLabelCount();
    auto allLabels = manager_.getAllLabels();
    EXPECT_EQ(allLabels.size(), count);
}

TEST_F(SegmentationConcurrencyTest, LabelManagerConcurrentReadDuringWrite) {
    constexpr int kWriteOps = 50;
    constexpr int kReaders = 4;

    std::latch startLatch(1 + kReaders);
    std::atomic<bool> writeDone{false};

    std::thread writer([&] {
        startLatch.arrive_and_wait();
        for (int i = 0; i < kWriteOps; ++i) {
            [[maybe_unused]] auto r = manager_.addLabel(
                "rw_" + std::to_string(i));
        }
        writeDone.store(true);
    });

    std::vector<std::thread> readers;
    std::atomic<int> totalReads{0};

    for (int r = 0; r < kReaders; ++r) {
        readers.emplace_back([&] {
            startLatch.arrive_and_wait();
            while (!writeDone.load()) {
                [[maybe_unused]] auto count = manager_.getLabelCount();
                [[maybe_unused]] auto labels = manager_.getAllLabels();
                [[maybe_unused]] auto active = manager_.getActiveLabel();
                totalReads.fetch_add(1);
                std::this_thread::yield();
            }
        });
    }

    writer.join();
    for (auto& t : readers) t.join();

    EXPECT_GT(totalReads.load(), 0);
    EXPECT_EQ(manager_.getLabelCount(), static_cast<size_t>(kWriteOps));
}

TEST_F(SegmentationConcurrencyTest, ManualSegmentationConcurrentToolSwitch) {
    ManualSegmentationController controller;
    auto initResult = controller.initializeLabelMap(64, 64, 64);
    ASSERT_TRUE(initResult.has_value());

    constexpr int kThreadCount = 4;
    constexpr int kSwitchesPerThread = 100;

    const SegmentationTool tools[] = {
        SegmentationTool::Brush,
        SegmentationTool::Eraser,
        SegmentationTool::Fill,
        SegmentationTool::Freehand,
    };

    std::latch startLatch(kThreadCount);
    std::atomic<int> completedOps{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i] {
            startLatch.arrive_and_wait();
            for (int j = 0; j < kSwitchesPerThread; ++j) {
                controller.setActiveTool(tools[(i + j) % 4]);
                controller.setBrushSize((j % 50) + 1);
                [[maybe_unused]] auto tool = controller.getActiveTool();
                [[maybe_unused]] auto size = controller.getBrushSize();
                completedOps.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(completedOps.load(), kThreadCount * kSwitchesPerThread);

    auto finalTool = controller.getActiveTool();
    EXPECT_NE(finalTool, SegmentationTool::None);
}

TEST_F(SegmentationConcurrencyTest, ConcurrentSegmentationOnSeparateVolumes) {
    constexpr int kThreadCount = 3;

    std::latch startLatch(kThreadCount);
    std::atomic<int> successCount{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i] {
            auto volume = createSyntheticCTVolume(64);

            RegionGrowingSegmenter segmenter;
            std::vector<SeedPoint> seeds = {{32, 32, 32}};

            startLatch.arrive_and_wait();

            auto result = segmenter.connectedThreshold(
                volume, seeds, -200.0, 200.0);

            if (result.has_value()) {
                successCount.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(successCount.load(), kThreadCount);
}

TEST_F(SegmentationConcurrencyTest, MorphologicalProcessingWhileReadingLabels) {
    // Pre-populate labels for concurrent reading
    for (int i = 0; i < 10; ++i) {
        auto result = manager_.addLabel("morph_" + std::to_string(i));
        ASSERT_TRUE(result.has_value());
    }

    auto mask = createBinaryMaskVolume(64, 20.0);

    std::latch startLatch(2);
    std::atomic<bool> morphDone{false};
    std::atomic<int> readCount{0};

    // Thread 1: Run morphological processing
    std::thread morphThread([&] {
        MorphologicalProcessor processor;

        startLatch.arrive_and_wait();

        [[maybe_unused]] auto dilated = processor.apply(
            mask, MorphologicalOperation::Dilation, 2);
        [[maybe_unused]] auto eroded = processor.apply(
            mask, MorphologicalOperation::Erosion, 2);

        morphDone.store(true);
    });

    // Thread 2: Read label manager state concurrently
    std::thread readerThread([&] {
        startLatch.arrive_and_wait();

        while (!morphDone.load()) {
            [[maybe_unused]] auto count = manager_.getLabelCount();
            [[maybe_unused]] auto labels = manager_.getAllLabels();
            readCount.fetch_add(1);
            std::this_thread::yield();
        }
    });

    morphThread.join();
    readerThread.join();

    EXPECT_GT(readCount.load(), 0);
    EXPECT_EQ(manager_.getLabelCount(), 10u);
}

// =============================================================================
// Processing Concurrency Tests
// =============================================================================

class ProcessingConcurrencyTest : public ::testing::Test {};

TEST_F(ProcessingConcurrencyTest, GaussianSmootherParallelInstances) {
    constexpr int kThreadCount = 3;

    std::latch startLatch(kThreadCount);
    std::atomic<int> successCount{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i] {
            auto volume = createSyntheticCTVolume(64);
            GaussianSmoother smoother;
            GaussianSmoother::Parameters params;
            params.variance = 1.0 + i * 0.5;

            startLatch.arrive_and_wait();

            auto result = smoother.apply(volume, params);
            if (result.has_value()) {
                successCount.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(successCount.load(), kThreadCount);
}

TEST_F(ProcessingConcurrencyTest, ConcurrentFilterPipeline) {
    std::latch startLatch(2);
    std::atomic<int> successCount{0};

    // Pipeline A: Gaussian smoothing
    std::thread smoothThread([&] {
        auto volume = createSyntheticCTVolume(64);
        GaussianSmoother smoother;

        startLatch.arrive_and_wait();

        auto result = smoother.apply(volume);
        if (result.has_value()) successCount.fetch_add(1);
    });

    // Pipeline B: Region growing segmentation
    std::thread segThread([&] {
        auto volume = createSyntheticCTVolume(64);
        RegionGrowingSegmenter segmenter;
        std::vector<SeedPoint> seeds = {{32, 32, 32}};

        startLatch.arrive_and_wait();

        auto result =
            segmenter.connectedThreshold(volume, seeds, -200.0, 200.0);
        if (result.has_value()) successCount.fetch_add(1);
    });

    smoothThread.join();
    segThread.join();
    EXPECT_EQ(successCount.load(), 2);
}

// =============================================================================
// Flow Concurrency Tests
// =============================================================================

class FlowConcurrencyTest : public ::testing::Test {};

TEST_F(FlowConcurrencyTest, PhaseCacheConcurrentGetPhase) {
    PhaseCache cache(5);
    cache.setTotalPhases(10);

    cache.setPhaseLoader(
        [](int phaseIndex) -> std::expected<VelocityPhase, FlowError> {
            auto [phase, truth] = test_utils::generatePoiseuillePipe(
                16, 50.0, 6.0, phaseIndex);
            return phase;
        });

    constexpr int kThreadCount = 4;
    std::latch startLatch(kThreadCount);
    std::atomic<int> successCount{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i] {
            startLatch.arrive_and_wait();
            for (int p = 0; p < 5; ++p) {
                int phaseIdx = (i + p) % 10;
                auto result = cache.getPhase(phaseIdx);
                if (result.has_value()) {
                    successCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(successCount.load(), kThreadCount * 5);
}

TEST_F(FlowConcurrencyTest, TemporalNavigatorConcurrentNavigation) {
    TemporalNavigator navigator;
    navigator.initialize(10, 50.0, 5);

    navigator.setPhaseLoader(
        [](int phaseIndex) -> std::expected<VelocityPhase, FlowError> {
            auto [phase, truth] = test_utils::generatePoiseuillePipe(
                16, 50.0, 6.0, phaseIndex);
            return phase;
        });

    constexpr int kThreadCount = 4;
    std::latch startLatch(kThreadCount);
    std::atomic<int> completedOps{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i] {
            startLatch.arrive_and_wait();
            for (int j = 0; j < 10; ++j) {
                int phase = (i * 10 + j) % 10;
                [[maybe_unused]] auto result = navigator.goToPhase(phase);
                completedOps.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(completedOps.load(), kThreadCount * 10);

    // Navigator should be in a valid state
    int currentPhase = navigator.currentPhase();
    EXPECT_GE(currentPhase, 0);
    EXPECT_LT(currentPhase, 10);
}

TEST_F(FlowConcurrencyTest, TemporalNavigatorPlayPauseStopConcurrency) {
    TemporalNavigator navigator;
    navigator.initialize(10, 50.0, 5);

    navigator.setPhaseLoader(
        [](int phaseIndex) -> std::expected<VelocityPhase, FlowError> {
            auto [phase, truth] = test_utils::generatePoiseuillePipe(
                16, 50.0, 6.0, phaseIndex);
            return phase;
        });

    constexpr int kThreadCount = 3;
    std::latch startLatch(kThreadCount);
    std::atomic<int> completedOps{0};

    // Thread 1: play/pause cycles
    std::thread playPauseThread([&] {
        startLatch.arrive_and_wait();
        for (int i = 0; i < 20; ++i) {
            navigator.play(15.0);
            std::this_thread::yield();
            navigator.pause();
            completedOps.fetch_add(1);
        }
    });

    // Thread 2: stop/play cycles
    std::thread stopPlayThread([&] {
        startLatch.arrive_and_wait();
        for (int i = 0; i < 20; ++i) {
            navigator.stop();
            std::this_thread::yield();
            navigator.play(30.0);
            completedOps.fetch_add(1);
        }
    });

    // Thread 3: state queries
    std::thread queryThread([&] {
        startLatch.arrive_and_wait();
        for (int i = 0; i < 50; ++i) {
            [[maybe_unused]] auto state = navigator.playbackState();
            [[maybe_unused]] int phase = navigator.currentPhase();
            completedOps.fetch_add(1);
        }
    });

    playPauseThread.join();
    stopPlayThread.join();
    queryThread.join();

    EXPECT_EQ(completedOps.load(), 20 + 20 + 50);
}

TEST_F(FlowConcurrencyTest, FlowQuantifierConcurrentMeasurements) {
    constexpr int kThreadCount = 3;

    std::latch startLatch(kThreadCount);
    std::atomic<int> successCount{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i] {
            FlowQuantifier quantifier;
            MeasurementPlane plane;
            plane.center = {15.5, 15.5, 15.5};
            plane.normal = {0.0, 0.0, 1.0};
            plane.radius = 12.0;
            plane.sampleSpacing = 1.0;
            quantifier.setMeasurementPlane(plane);

            auto [phase, truth] = test_utils::generatePoiseuillePipe(
                32, 80.0 + i * 10.0, 10.0, i);

            startLatch.arrive_and_wait();

            auto result = quantifier.measureFlow(phase);
            if (result.has_value()) {
                successCount.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(successCount.load(), kThreadCount);
}

// =============================================================================
// Stress Concurrency Tests
// =============================================================================

class StressConcurrencyTest : public ::testing::Test {};

TEST_F(StressConcurrencyTest, RapidServiceCreationDestruction) {
    // Warm up ITK object factories and logger singletons in main thread
    // before concurrent access. ITK's factory registration uses global
    // state that is not thread-safe for first-time initialization.
    {
        LabelManager warmupManager;
        [[maybe_unused]] auto r = warmupManager.initializeLabelMap(8, 8, 8);
    }
    {
        GaussianSmoother warmupSmoother;
        auto warmupVol = createVolume(8);
        [[maybe_unused]] auto r = warmupSmoother.apply(warmupVol);
    }

    constexpr int kThreadCount = 4;
    constexpr int kCycles = 50;

    std::latch startLatch(kThreadCount);
    std::atomic<int> completedCycles{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&] {
            startLatch.arrive_and_wait();
            for (int j = 0; j < kCycles; ++j) {
                {
                    LabelManager manager;
                    [[maybe_unused]] auto r =
                        manager.initializeLabelMap(16, 16, 16);
                    if (r.has_value()) {
                        [[maybe_unused]] auto l = manager.addLabel("test");
                    }
                }
                {
                    GaussianSmoother smoother;
                    auto vol = createVolume(16);
                    [[maybe_unused]] auto r = smoother.apply(vol);
                }
                completedCycles.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(completedCycles.load(), kThreadCount * kCycles);
}

TEST_F(StressConcurrencyTest, HighContentionLabelManager) {
    LabelManager manager;
    auto initResult = manager.initializeLabelMap(32, 32, 32);
    ASSERT_TRUE(initResult.has_value());

    constexpr int kThreadCount = 8;
    constexpr int kOpsPerThread = 50;

    std::latch startLatch(kThreadCount);
    std::atomic<int> completedOps{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i] {
            startLatch.arrive_and_wait();
            for (int j = 0; j < kOpsPerThread; ++j) {
                switch (j % 4) {
                    case 0: {
                        [[maybe_unused]] auto r = manager.addLabel(
                            "t" + std::to_string(i) + "_" +
                            std::to_string(j));
                        break;
                    }
                    case 1: {
                        auto labels = manager.getAllLabels();
                        if (!labels.empty()) {
                            [[maybe_unused]] auto r =
                                manager.removeLabel(labels.front().id, false);
                        }
                        break;
                    }
                    case 2: {
                        [[maybe_unused]] auto c = manager.getLabelCount();
                        [[maybe_unused]] auto a = manager.getActiveLabel();
                        break;
                    }
                    case 3: {
                        auto labels = manager.getAllLabels();
                        if (!labels.empty()) {
                            [[maybe_unused]] auto r =
                                manager.setActiveLabel(labels.back().id);
                        }
                        break;
                    }
                }
                completedOps.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(completedOps.load(), kThreadCount * kOpsPerThread);

    // Verify internal consistency
    auto labels = manager.getAllLabels();
    EXPECT_EQ(labels.size(), manager.getLabelCount());
}

TEST_F(StressConcurrencyTest, ConcurrentVolumeAllocationAndProcessing) {
    constexpr int kThreadCount = 4;

    std::latch startLatch(kThreadCount);
    std::atomic<int> successCount{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i] {
            startLatch.arrive_and_wait();

            auto volume = createSyntheticCTVolume(64);
            EXPECT_NE(volume, nullptr);

            GaussianSmoother smoother;
            GaussianSmoother::Parameters params;
            params.variance = 1.0 + i * 0.5;

            auto result = smoother.apply(volume, params);
            if (result.has_value()) {
                EXPECT_NE(result.value(), nullptr);
                successCount.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_EQ(successCount.load(), kThreadCount);
}

}  // namespace
}  // namespace dicom_viewer::services
