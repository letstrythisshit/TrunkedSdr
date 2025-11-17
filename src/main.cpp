#include "utils/logger.h"
#include "utils/config_parser.h"
#include "trunking/trunk_controller.h"
#include "sdr/rtlsdr_source.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

using namespace TrunkSDR;

std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal..." << std::endl;
        g_running = false;
    }
}

void printBanner() {
    std::cout << R"(
╔═══════════════════════════════════════════════════════════╗
║                      TrunkSDR v1.0                        ║
║          Trunked Radio System Decoder for ARM             ║
║                                                           ║
║  Supports: P25, SmartNet, EDACS, DMR, and more           ║
╚═══════════════════════════════════════════════════════════╝
)" << std::endl;
}

void printUsage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n"
              << "\n"
              << "Options:\n"
              << "  -c, --config FILE    Configuration file (default: config.json)\n"
              << "  -l, --log-level LVL  Log level: debug, info, warning, error (default: info)\n"
              << "  -f, --log-file FILE  Log to file instead of stdout\n"
              << "  -d, --devices        List available RTL-SDR devices and exit\n"
              << "  -h, --help           Show this help message\n"
              << "\n"
              << "Example:\n"
              << "  " << prog_name << " --config /etc/trunksdr/config.json\n"
              << std::endl;
}

void listDevices() {
    uint32_t count = RTLSDRSource::getDeviceCount();

    std::cout << "Found " << count << " RTL-SDR device(s):\n" << std::endl;

    for (uint32_t i = 0; i < count; i++) {
        std::string name = RTLSDRSource::getDeviceName(i);
        std::cout << "  [" << i << "] " << name << std::endl;
    }

    std::cout << std::endl;
}

void printSystemInfo(const Config& config) {
    std::cout << "System Information:" << std::endl;
    std::cout << "  Type: " << ConfigParser::systemTypeToString(config.system.type) << std::endl;
    std::cout << "  Name: " << config.system.name << std::endl;

    if (config.system.system_id != 0) {
        std::cout << "  System ID: 0x" << std::hex << config.system.system_id << std::dec << std::endl;
    }

    if (config.system.nac != 0) {
        std::cout << "  NAC: 0x" << std::hex << config.system.nac << std::dec << std::endl;
    }

    std::cout << "  Control Channels: ";
    for (size_t i = 0; i < config.system.control_channels.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << config.system.control_channels[i] / 1e6 << " MHz";
    }
    std::cout << std::endl;

    std::cout << "  Enabled Talkgroups: " << config.talkgroups.enabled.size() << std::endl;

    std::cout << "\nAudio Configuration:" << std::endl;
    std::cout << "  Output Device: " << config.audio.output_device << std::endl;
    std::cout << "  Sample Rate: " << config.audio.sample_rate << " Hz" << std::endl;
    std::cout << "  Recording: " << (config.audio.record_calls ? "enabled" : "disabled") << std::endl;

    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    printBanner();

    // Default configuration
    std::string config_file = "config.json";
    std::string log_level = "info";
    std::string log_file;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-d" || arg == "--devices") {
            listDevices();
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_file = argv[++i];
            } else {
                std::cerr << "Error: --config requires a filename" << std::endl;
                return 1;
            }
        } else if (arg == "-l" || arg == "--log-level") {
            if (i + 1 < argc) {
                log_level = argv[++i];
            } else {
                std::cerr << "Error: --log-level requires a level" << std::endl;
                return 1;
            }
        } else if (arg == "-f" || arg == "--log-file") {
            if (i + 1 < argc) {
                log_file = argv[++i];
            } else {
                std::cerr << "Error: --log-file requires a filename" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Set up logging
    if (log_level == "debug") {
        Logger::instance().setLogLevel(LogLevel::DEBUG);
    } else if (log_level == "info") {
        Logger::instance().setLogLevel(LogLevel::INFO);
    } else if (log_level == "warning") {
        Logger::instance().setLogLevel(LogLevel::WARNING);
    } else if (log_level == "error") {
        Logger::instance().setLogLevel(LogLevel::ERROR);
    }

    if (!log_file.empty()) {
        Logger::instance().setLogFile(log_file);
    }

    LOG_INFO("TrunkSDR starting up...");
    LOG_INFO("Configuration file:", config_file);

    // Load configuration
    ConfigParser parser;
    if (!parser.loadFromFile(config_file)) {
        LOG_CRITICAL("Failed to load configuration file:", config_file);
        std::cerr << "Failed to load configuration. Please check your config file." << std::endl;
        return 1;
    }

    const Config& config = parser.getConfig();
    printSystemInfo(config);

    // Check for RTL-SDR devices
    uint32_t device_count = RTLSDRSource::getDeviceCount();
    if (device_count == 0) {
        LOG_CRITICAL("No RTL-SDR devices found!");
        std::cerr << "No RTL-SDR devices detected. Please connect a device and try again." << std::endl;
        return 1;
    }

    LOG_INFO("Found", device_count, "RTL-SDR device(s)");

    // Initialize trunk controller
    TrunkController controller;
    if (!controller.initialize(config)) {
        LOG_CRITICAL("Failed to initialize trunk controller");
        std::cerr << "Initialization failed. Check logs for details." << std::endl;
        return 1;
    }

    // Start the controller
    if (!controller.start()) {
        LOG_CRITICAL("Failed to start trunk controller");
        std::cerr << "Failed to start. Check logs for details." << std::endl;
        return 1;
    }

    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "TrunkSDR is running. Press Ctrl+C to stop." << std::endl;
    std::cout << "Monitoring control channel..." << std::endl;

    // Main loop
    auto last_status = std::chrono::steady_clock::now();
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Print status every 10 seconds
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_status).count() >= 10) {
            CallManager* call_mgr = controller.getCallManager();
            if (call_mgr) {
                size_t active_calls = call_mgr->getActiveCallCount();
                uint64_t total_calls = call_mgr->getTotalCallCount();

                std::cout << "Status: Active calls: " << active_calls
                         << " | Total: " << total_calls << std::endl;
            }

            last_status = now;
        }
    }

    // Shutdown
    std::cout << "Shutting down..." << std::endl;
    controller.stop();

    LOG_INFO("TrunkSDR shutdown complete");
    std::cout << "Goodbye!" << std::endl;

    return 0;
}
