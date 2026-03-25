// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
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

/**
 * @file main.cpp
 * @brief Headless DICOM Viewer Server entry point
 * @details Bootstraps all services without Qt:
 *   1. CLI argument parsing
 *   2. spdlog initialization (console + file)
 *   3. VTK EGL off-screen rendering validation
 *   4. Service instantiation (RenderSessionManager, WebSocketFrameStreamer)
 *   5. Frame + input callback wiring
 *   6. REST API server (ApiServer) on port 8080
 *   7. WebSocket server (WebSocketFrameStreamer) on port 8081
 *   8. SIGTERM/SIGINT graceful shutdown
 *
 * @author kcenon
 * @since 1.0.0
 */

#include "api/api_server.hpp"

#include "services/render/render_session_manager.hpp"
#include "services/render/websocket_frame_streamer.hpp"
#include "services/render/frame_encoder.hpp"
#include "services/render/input_event_dispatcher.hpp"
#include "services/render/offscreen_render_context.hpp"
#include "services/render/session_token_validator.hpp"
#include "services/audit_service.hpp"
#include "services/config/deployment_config.hpp"
#include "services/store/session_store.hpp"
#ifdef DICOM_VIEWER_HAS_HIREDIS
#include "services/store/redis_session_store.hpp"
#endif
#ifdef DICOM_VIEWER_HAS_LIBPQ
#include "services/store/postgres_audit_sink.hpp"
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

// ---- CLI argument structure ----

struct ServerArgs {
    uint16_t restPort = 8080;
    uint16_t wsPort = 8081;
    std::string configPath;
    uint32_t maxSessions = 8;
    std::string logLevel = "info";
    bool helpRequested = false;
    std::string redisHost;
    uint16_t redisPort = 6379;
    std::string redisPassword;
    std::string pgHost;
    uint16_t pgPort = 5432;
    std::string pgDatabase = "dicom_viewer";
    std::string pgUser = "dicom_viewer";
    std::string pgPassword;
};

// ---- Signal handling ----

static std::atomic<bool> g_shutdown{false};
static std::mutex g_shutdownMutex;
static std::condition_variable g_shutdownCv;

static void signalHandler(int /*signum*/) {
    g_shutdown = true;
    g_shutdownCv.notify_all();
}

// ---- Helpers ----

static void printHelp(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --port <port>          REST API listen port (default: 8080)\n"
              << "  --ws-port <port>       WebSocket listen port (default: 8081)\n"
              << "  --config <path>        Path to deployment.yaml\n"
              << "  --max-sessions <n>     Maximum concurrent render sessions (default: 8)\n"
              << "  --log-level <level>    Log level: trace|debug|info|warn|error (default: info)\n"
              << "  --redis-host <host>    Redis host for session persistence (optional)\n"
              << "  --redis-port <port>    Redis port (default: 6379)\n"
              << "  --redis-password <pw>  Redis AUTH password (optional)\n"
              << "  --pg-host <host>       PostgreSQL host for audit log persistence (optional)\n"
              << "  --pg-port <port>       PostgreSQL port (default: 5432)\n"
              << "  --pg-database <name>   PostgreSQL database name (default: dicom_viewer)\n"
              << "  --pg-user <user>       PostgreSQL user (default: dicom_viewer)\n"
              << "  --pg-password <pw>     PostgreSQL password (optional)\n"
              << "  --help, -h             Show this help message\n\n"
              << "Examples:\n"
              << "  " << programName << " --port 8080 --ws-port 8081\n"
              << "  " << programName << " --config /etc/dicom_viewer/deployment.yaml\n";
}

static ServerArgs parseArgs(int argc, char* argv[]) {
    ServerArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto nextArg = [&]() -> std::string {
            if (i + 1 < argc) return argv[++i];
            std::cerr << "Error: " << arg << " requires an argument\n";
            std::exit(EXIT_FAILURE);
        };

        if (arg == "--help" || arg == "-h") {
            args.helpRequested = true;
        } else if (arg == "--port") {
            args.restPort = static_cast<uint16_t>(std::stoi(nextArg()));
        } else if (arg == "--ws-port") {
            args.wsPort = static_cast<uint16_t>(std::stoi(nextArg()));
        } else if (arg == "--config") {
            args.configPath = nextArg();
        } else if (arg == "--max-sessions") {
            args.maxSessions = static_cast<uint32_t>(std::stoi(nextArg()));
        } else if (arg == "--log-level") {
            args.logLevel = nextArg();
        } else if (arg == "--redis-host") {
            args.redisHost = nextArg();
        } else if (arg == "--redis-port") {
            args.redisPort = static_cast<uint16_t>(std::stoi(nextArg()));
        } else if (arg == "--redis-password") {
            args.redisPassword = nextArg();
        } else if (arg == "--pg-host") {
            args.pgHost = nextArg();
        } else if (arg == "--pg-port") {
            args.pgPort = static_cast<uint16_t>(std::stoi(nextArg()));
        } else if (arg == "--pg-database") {
            args.pgDatabase = nextArg();
        } else if (arg == "--pg-user") {
            args.pgUser = nextArg();
        } else if (arg == "--pg-password") {
            args.pgPassword = nextArg();
        } else {
            std::cerr << "Warning: unknown argument '" << arg << "'\n";
        }
    }
    return args;
}

static spdlog::level::level_enum parseSpdlogLevel(const std::string& level) {
    if (level == "trace")  return spdlog::level::trace;
    if (level == "debug")  return spdlog::level::debug;
    if (level == "warn")   return spdlog::level::warn;
    if (level == "error")  return spdlog::level::err;
    return spdlog::level::info;  // default
}

static void initializeLogging(const std::string& logLevel) {
    const auto level = parseSpdlogLevel(logLevel);

    // Console sink
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(level);

    // Rotating file sink (10 MB per file, 3 rotations)
    const std::filesystem::path logDir = std::filesystem::temp_directory_path() / "dicom_viewer";
    std::filesystem::create_directories(logDir);
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        (logDir / "server.log").string(), 10 * 1024 * 1024, 3);
    fileSink->set_level(level);

    auto logger = std::make_shared<spdlog::logger>(
        "server", spdlog::sinks_init_list{consoleSink, fileSink});
    logger->set_level(level);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::warn);
}

static bool validateVtkHeadless() {
    // Set VTK headless environment variable for EGL/OSMesa selection
#if defined(__linux__)
    ::setenv("VTK_DEFAULT_RENDER_WINDOW_HEADLESS", "1", /*overwrite=*/0);
#endif

    // Attempt to create a minimal off-screen context to validate the backend
    try {
        dicom_viewer::services::OffscreenRenderContext ctx;
        ctx.initialize(64, 64);
        if (!ctx.isInitialized()) {
            spdlog::warn("VTK off-screen context failed to initialize — "
                         "rendering may not work in this environment");
            return false;
        }
        spdlog::info("VTK off-screen rendering validated (supportsOpenGL={})",
                     ctx.supportsOpenGL());
        return true;
    } catch (const std::exception& ex) {
        spdlog::warn("VTK off-screen validation threw: {}", ex.what());
        return false;
    }
}

// ---- Main ----

int main(int argc, char* argv[]) {
    ServerArgs args = parseArgs(argc, argv);

    if (args.helpRequested) {
        printHelp(argv[0]);
        return EXIT_SUCCESS;
    }

    // 1. Logging
    initializeLogging(args.logLevel);
    spdlog::info("dicom_viewer starting (REST:{} WS:{} maxSessions:{})",
                 args.restPort, args.wsPort, args.maxSessions);

    // 2. VTK headless validation (non-fatal — warn and continue)
    validateVtkHeadless();

    // 3. Load deployment config (if --config provided, overrides CLI args)

    std::optional<dicom_viewer::services::DeploymentConfig> deployCfg;
    if (!args.configPath.empty()) {
        auto result = dicom_viewer::services::loadDeploymentConfig(args.configPath);
        if (result) {
            deployCfg = std::move(*result);
            spdlog::info("Deployment config loaded from '{}'", args.configPath);

            // Override CLI args with deployment.yaml values
            if (args.restPort == 8080) args.restPort = deployCfg->server.restPort;
            if (args.wsPort == 8081) args.wsPort = deployCfg->server.wsPort;
            if (args.logLevel == "info") args.logLevel = deployCfg->server.logLevel;
            if (args.maxSessions == 8) args.maxSessions = deployCfg->server.maxSessions;
            if (args.redisHost.empty()) args.redisHost = deployCfg->redis.host;
            if (args.redisPort == 6379) args.redisPort = deployCfg->redis.port;
            if (args.redisPassword.empty()) args.redisPassword = deployCfg->redis.password;
            if (args.pgHost.empty()) args.pgHost = deployCfg->postgres.host;
            if (args.pgPort == 5432) args.pgPort = deployCfg->postgres.port;
            if (args.pgDatabase == "dicom_viewer") args.pgDatabase = deployCfg->postgres.database;
            if (args.pgUser == "dicom_viewer") args.pgUser = deployCfg->postgres.user;
            if (args.pgPassword.empty()) args.pgPassword = deployCfg->postgres.password;
        } else {
            spdlog::error("Failed to load deployment config: {}",
                         dicom_viewer::services::toString(result.error()));
            return EXIT_FAILURE;
        }
    }

    // 4. Service instantiation

    // JWT token validator (ephemeral keys for dev, key files for production)
    dicom_viewer::services::SessionTokenConfig tokenCfg;
    tokenCfg.allowEphemeralKeys = true;
    auto tokenValidator = std::make_unique<dicom_viewer::services::SessionTokenValidator>(tokenCfg);
    if (!tokenValidator) {
        spdlog::critical("Cannot start server: JWT validator not initialized");
        return EXIT_FAILURE;
    }

    // Audit service (disabled by default — enable via deployment.yaml)
    auto auditService = std::make_unique<dicom_viewer::services::AuditService>();

    // PostgreSQL audit sink (if configured via CLI or deployment.yaml)
#ifdef DICOM_VIEWER_HAS_LIBPQ
    if (!args.pgHost.empty()) {
        dicom_viewer::services::PostgresConfig pgCfg;
        pgCfg.host = args.pgHost;
        pgCfg.port = args.pgPort;
        pgCfg.database = args.pgDatabase;
        pgCfg.user = args.pgUser;
        pgCfg.password = args.pgPassword;
        if (deployCfg) pgCfg.sslMode = deployCfg->postgres.sslMode;
        auto pgSink = std::make_unique<dicom_viewer::services::PostgresAuditSink>(pgCfg);
        if (pgSink->isConnected()) {
            spdlog::info("Audit sink: PostgreSQL ({}:{}/{})",
                         args.pgHost, args.pgPort, args.pgDatabase);
            auditService->setAuditSink(std::move(pgSink));
        } else {
            spdlog::warn("PostgreSQL connection failed, audit events logged via spdlog only");
        }
    }
#endif

    // Frame encoder
    auto frameEncoder = std::make_unique<dicom_viewer::services::FrameEncoder>();

    // Input event dispatcher
    auto inputDispatcher = std::make_unique<dicom_viewer::services::InputEventDispatcher>();

    // Session store (Redis if configured via CLI or deployment.yaml, otherwise in-memory)
    std::unique_ptr<dicom_viewer::services::ISessionStore> sessionStore;
#ifdef DICOM_VIEWER_HAS_HIREDIS
    if (!args.redisHost.empty()) {
        dicom_viewer::services::RedisConfig redisCfg;
        redisCfg.host = args.redisHost;
        redisCfg.port = args.redisPort;
        redisCfg.password = args.redisPassword;
        auto redisStore = std::make_unique<dicom_viewer::services::RedisSessionStore>(redisCfg);
        if (redisStore->isConnected()) {
            spdlog::info("Session store: Redis ({}:{})", args.redisHost, args.redisPort);
            sessionStore = std::move(redisStore);
        } else {
            spdlog::warn("Redis connection failed, falling back to in-memory session store");
            sessionStore = std::make_unique<dicom_viewer::services::InMemorySessionStore>();
        }
    } else
#endif
    {
        spdlog::info("Session store: in-memory");
        sessionStore = std::make_unique<dicom_viewer::services::InMemorySessionStore>();
    }

    // Render session manager
    dicom_viewer::services::RenderSessionManagerConfig sessionCfg;
    sessionCfg.maxSessions = args.maxSessions;
    auto sessionManager = std::make_unique<dicom_viewer::services::RenderSessionManager>(sessionCfg);
    sessionManager->setSessionStore(sessionStore.get());

    // WebSocket frame streamer
    auto wsStreamer = std::make_unique<dicom_viewer::services::WebSocketFrameStreamer>();
    wsStreamer->setTokenValidator(tokenValidator.get());
    wsStreamer->setAuditService(auditService.get());

    // 4. Wire frame callback pipeline:
    //    RenderSessionManager → FrameEncoder → WebSocketFrameStreamer
    sessionManager->setFrameReadyCallback(
        [&wsStreamer, &frameEncoder, frameSeq = uint32_t{0}]
        (const std::string& sessionId,
         const std::vector<uint8_t>& rgbaFrame,
         uint32_t width, uint32_t height) mutable {
            if (!wsStreamer->hasClients(sessionId)) return;
            const auto encoded = frameEncoder->encode(
                rgbaFrame.data(), width, height,
                dicom_viewer::services::EncodeFormat::Jpeg, 85);
            if (!encoded.empty()) {
                wsStreamer->pushFrame(sessionId, encoded, width, height, ++frameSeq);
            }
        });

    // 5. Wire input callback pipeline:
    //    WebSocketFrameStreamer → InputEventDispatcher → (VTK via RenderSession)
    wsStreamer->setInputEventCallback(
        [&inputDispatcher, &sessionManager](const dicom_viewer::services::InputEvent& event) {
            sessionManager->touchSession(event.sessionId);
            inputDispatcher->enqueue(event);
            // Dispatch to VTK interactor via RenderSession
            auto* session = sessionManager->getSession(event.sessionId);
            if (session) {
                sessionManager->notifyInteractionStart(event.sessionId);
            }
        });

    // 6. REST API server
    dicom_viewer::server::ApiServerConfig apiCfg;
    apiCfg.port = args.restPort;
    auto apiServer = std::make_unique<dicom_viewer::server::ApiServer>(apiCfg);
    apiServer->setServices(sessionManager.get(), tokenValidator.get(), auditService.get());

    if (!apiServer->start()) {
        spdlog::error("Failed to start REST API server on port {}", args.restPort);
        return EXIT_FAILURE;
    }

    // 7. WebSocket server
    dicom_viewer::services::WebSocketStreamConfig wsCfg;
    wsCfg.port = args.wsPort;
    if (!wsStreamer->start(wsCfg)) {
        spdlog::error("Failed to start WebSocket server on port {}", args.wsPort);
        apiServer->stop();
        return EXIT_FAILURE;
    }

    // 8. Start render loop
    sessionManager->startRenderLoop();

    spdlog::info("Server ready — REST API: http://0.0.0.0:{}/api/v1/health",
                 args.restPort);
    spdlog::info("              WebSocket: ws://0.0.0.0:{}/render/{{session_id}}",
                 args.wsPort);

    // 9. Signal handling for graceful shutdown
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT,  signalHandler);

    {
        std::unique_lock<std::mutex> lock(g_shutdownMutex);
        g_shutdownCv.wait(lock, [] { return g_shutdown.load(); });
    }

    // 10. Graceful shutdown
    spdlog::info("Shutdown signal received — stopping services");

    sessionManager->stopRenderLoop();
    wsStreamer->stop();
    apiServer->stop();

    spdlog::info("dicom_viewer stopped cleanly");
    return EXIT_SUCCESS;
}
