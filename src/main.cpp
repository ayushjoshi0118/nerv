#include "app/application.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::cout << "=========================================================\n";
    std::cout << " NERV \xE2\x80\x94 The Nervous System of Your Computer\n";
    std::cout << " \"Your machine is alive. See what it's thinking.\"\n";
    std::cout << "=========================================================\n";

    nerv::ApplicationConfig config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mock") {
            config.use_mock_telemetry = true;
            std::cout << "[NERV] Running with Mock Telemetry Backend\n";
        } else if (arg == "--width" && i + 1 < argc) {
            config.window_width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            config.window_height = std::stoi(argv[++i]);
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
