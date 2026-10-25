#include "app/application.hpp"
#include <iostream>
#include <string>

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  --help, -h          Display this help message and exit\n"
              << "  --mock              Use synthetic mock telemetry backend (offline demo / testing)\n"
              << "  --width <pixels>    Set initial window width (default: 1600)\n"
              << "  --height <pixels>   Set initial window height (default: 900)\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=========================================================\n";
    std::cout << " NERV \xE2\x80\x94 The Nervous System of Your Computer\n";
    std::cout << " \"Your machine is alive. See what it's thinking.\"\n";
    std::cout << "=========================================================\n";

    nerv::ApplicationConfig config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--mock") {
            config.use_mock_telemetry = true;
            std::cout << "[NERV] Running with Mock Telemetry Backend\n";
        } else if (arg == "--width" && i + 1 < argc) {
            try {
                int w = std::stoi(argv[++i]);
                if (w >= 640 && w <= 7680) config.window_width = w;
                else std::cerr << "[NERV Warning] Width " << w << " out of range [640, 7680]. Using default.\n";
            } catch (...) {
                std::cerr << "[NERV Warning] Invalid --width argument, using default.\n";
            }
        } else if (arg == "--height" && i + 1 < argc) {
            try {
                int h = std::stoi(argv[++i]);
                if (h >= 480 && h <= 4320) config.window_height = h;
                else std::cerr << "[NERV Warning] Height " << h << " out of range [480, 4320]. Using default.\n";
            } catch (...) {
                std::cerr << "[NERV Warning] Invalid --height argument, using default.\n";
            }
        }
    }

    nerv::Application app(config);
    if (!app.initialize()) {
        std::cerr << "[NERV] Application initialization failed!\n";
        return 1;
    }

    std::cout << "[NERV] Starting main graphics and telemetry loop...\n";
    app.run();

    std::cout << "[NERV] Clean shutdown complete.\n";
    return 0;
}
