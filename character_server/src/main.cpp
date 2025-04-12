#include "server_instance.h"
#include <csignal>
#include <iostream>

std::function<void(int)> shutdownHandler;
void signalHandler(int signal) { shutdownHandler(signal); }

int main() {
    try {
        auto& server = ServerInstance::getInstance();

        // Setup signal handling
        shutdownHandler = [&](int signal) {
            std::cout << "\nShutting down server..." << std::endl;
            server.stop();
        };

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        if (!server.initialize(Protocol::PORT)) {
            std::cerr << "Failed to initialize server" << std::endl;
            return 1;
        }

        server.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
