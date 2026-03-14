// Headless DICOM Viewer Server
// Full implementation: see issue #500 (feat(server): create headless server project structure)
//
// This stub validates that the server-only build target compiles correctly
// without Qt dependencies.

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
            std::cout << "dicom_viewer_server [OPTIONS]\n"
                      << "  --port <port>          REST API port (default: 8080)\n"
                      << "  --ws-port <port>       WebSocket port (default: 8081)\n"
                      << "  --config <path>        Path to deployment.yaml\n"
                      << "  --max-sessions <n>     Maximum concurrent sessions\n"
                      << "  --log-level <level>    Log level (trace/debug/info/warn/error)\n"
                      << "  --help                 Show this help message\n";
            return EXIT_SUCCESS;
        }
    }

    std::cerr << "dicom_viewer_server: full implementation pending (issue #500)\n";
    return EXIT_FAILURE;
}
