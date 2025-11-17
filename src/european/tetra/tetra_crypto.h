#ifndef TETRA_CRYPTO_H
#define TETRA_CRYPTO_H

#include <cstdint>
#include <vector>
#include <string>

namespace TrunkSDR {
namespace European {

/**
 * ⚠️ CRITICAL LEGAL WARNING ⚠️
 *
 * This module implements cryptanalysis of TETRA TEA1 encryption based on
 * publicly disclosed vulnerabilities (CVE-2022-24402) discovered by Midnight Blue
 * in 2023.
 *
 * LEGAL RESTRICTIONS:
 * - This code is for EDUCATIONAL and AUTHORIZED SECURITY RESEARCH ONLY
 * - Unauthorized interception of encrypted communications is ILLEGAL in most jurisdictions
 * - Use only with explicit written authorization
 * - Users are SOLELY RESPONSIBLE for compliance with all applicable laws
 *
 * TECHNICAL BACKGROUND:
 * TEA1 contains an intentional backdoor that reduces the effective key space
 * from 80 bits to approximately 32 bits, making it vulnerable to brute-force
 * attacks on consumer hardware in reasonable time (~1 minute).
 *
 * TEA2, TEA3, and TEA4 are NOT vulnerable to these attacks.
 *
 * References:
 * - CVE-2022-24402: TEA1 Intentional Backdoor
 * - Midnight Blue "TETRA:BURST" research (2023)
 * - https://www.midnightblue.nl/research/tetraburst
 */

// Encryption algorithm identifiers
enum class TETRAEncryptionAlgorithm {
    NONE = 0,
    TEA1 = 1,     // VULNERABLE - CVE-2022-24402
    TEA2 = 2,     // Secure - no known vulnerabilities
    TEA3 = 3,     // Secure - no known vulnerabilities
    TEA4 = 4,     // Secure - no known vulnerabilities
    UNKNOWN = 255
};

// Key recovery result
struct TEA1KeyRecoveryResult {
    bool success;
    uint32_t recovered_key;      // 32-bit effective key
    uint64_t attempts;           // Number of attempts made
    double time_seconds;         // Time taken
    std::string error_message;
};

// Decryption result
struct TETRADecryptionResult {
    bool success;
    std::vector<uint8_t> plaintext;
    TETRAEncryptionAlgorithm algorithm;
    std::string error_message;
};

/**
 * TETRA Cryptographic Operations Handler
 *
 * Implements:
 * - TEA1 key recovery (exploiting CVE-2022-24402)
 * - TEA1 decryption once key is recovered
 * - Detection of encryption algorithms
 *
 * Does NOT implement:
 * - TEA2/TEA3/TEA4 attacks (these are secure)
 * - Key database management (user must implement)
 */
class TETRACrypto {
public:
    TETRACrypto();
    ~TETRACrypto() = default;

    /**
     * Detect encryption algorithm from TETRA burst
     * @param burst_data Raw TETRA burst data
     * @param length Length of burst data
     * @return Detected encryption algorithm
     */
    TETRAEncryptionAlgorithm detectEncryption(const uint8_t* burst_data, size_t length);

    /**
     * Attempt to recover TEA1 key using known vulnerability
     *
     * This exploits CVE-2022-24402, the intentional backdoor in TEA1 that
     * reduces the effective key space to 32 bits.
     *
     * @param ciphertext Encrypted TETRA burst
     * @param ciphertext_len Length of ciphertext
     * @param known_plaintext Known plaintext (if available)
     * @param known_plaintext_len Length of known plaintext
     * @return Key recovery result
     */
    TEA1KeyRecoveryResult recoverTEA1Key(
        const uint8_t* ciphertext,
        size_t ciphertext_len,
        const uint8_t* known_plaintext = nullptr,
        size_t known_plaintext_len = 0
    );

    /**
     * Decrypt TEA1 encrypted data using recovered key
     * @param ciphertext Encrypted data
     * @param ciphertext_len Length of ciphertext
     * @param key_32bit 32-bit recovered key
     * @return Decrypted plaintext
     */
    TETRADecryptionResult decryptTEA1(
        const uint8_t* ciphertext,
        size_t ciphertext_len,
        uint32_t key_32bit
    );

    /**
     * Add known key to key cache for faster decryption
     * @param network_id TETRA network identifier (MCC+MNC)
     * @param talkgroup Talkgroup ID
     * @param key_32bit Recovered 32-bit key
     */
    void addKnownKey(uint32_t network_id, uint32_t talkgroup, uint32_t key_32bit);

    /**
     * Check if key is already known for this network/talkgroup
     * @param network_id TETRA network identifier
     * @param talkgroup Talkgroup ID
     * @param out_key Output parameter for key if found
     * @return True if key is known
     */
    bool hasKnownKey(uint32_t network_id, uint32_t talkgroup, uint32_t* out_key);

    /**
     * Get statistics about key recovery operations
     */
    struct CryptoStats {
        size_t tea1_keys_recovered;
        size_t tea1_decryptions_successful;
        size_t tea1_decryptions_failed;
        size_t tea2_detected;  // Count of secure TEA2 detected
        size_t tea3_detected;  // Count of secure TEA3 detected
        double total_key_recovery_time;
    };

    CryptoStats getStats() const { return stats_; }

private:
    // TEA1 key recovery implementation (exploits CVE-2022-24402)
    uint32_t bruteForceTEA1Key(
        const uint8_t* ciphertext,
        size_t ciphertext_len,
        const uint8_t* known_plaintext,
        size_t known_plaintext_len,
        uint64_t* attempts_out
    );

    // TEA1 encryption/decryption primitives
    void tea1Encrypt(const uint8_t* plaintext, uint8_t* ciphertext, uint32_t key);
    void tea1Decrypt(const uint8_t* ciphertext, uint8_t* plaintext, uint32_t key);

    // Key expansion (converts 32-bit effective key to internal state)
    void expandTEA1Key(uint32_t key_32bit, uint32_t* expanded_key);

    // Verification helpers
    bool verifyDecryption(const uint8_t* plaintext, size_t length);

    // Key cache: (network_id << 24 | talkgroup) -> key
    std::map<uint64_t, uint32_t> key_cache_;

    // Statistics
    CryptoStats stats_;
};

/**
 * Legal compliance checker
 * Forces user to acknowledge legal responsibilities before use
 */
class TETRACryptoLegalChecker {
public:
    /**
     * Check if user has acknowledged legal warnings
     * If not, display warnings and require explicit acknowledgment
     * @return True if user has valid authorization
     */
    static bool checkAuthorization();

    /**
     * Display legal warning and require user acknowledgment
     * @return True if user acknowledges, false if user declines
     */
    static bool displayWarningAndGetAcknowledgment();

    /**
     * Check if authorization file exists (indicates prior acknowledgment)
     * @return True if authorized
     */
    static bool hasAuthorizationFile();

    /**
     * Create authorization file after user acknowledges
     */
    static void createAuthorizationFile();

    /**
     * Get path to authorization file
     */
    static std::string getAuthorizationFilePath();
};

} // namespace European
} // namespace TrunkSDR

#endif // TETRA_CRYPTO_H
