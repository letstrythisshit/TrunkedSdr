#ifndef TETRA_DECODER_H
#define TETRA_DECODER_H

#include "../../decoders/base_decoder.h"
#include "tetra_phy.h"
#ifdef ENABLE_TETRA_DECRYPTION
#include "tetra_crypto.h"
#endif
#include <map>
#include <set>

namespace TrunkSDR {
namespace European {

/**
 * TETRA (Terrestrial Trunked Radio) Decoder
 *
 * Main decoder class that:
 * - Coordinates physical and MAC layers
 * - Decodes control channels (MCCH, BSCH, BNCH)
 * - Extracts system information (MCC, MNC, color code)
 * - Detects call grants
 * - Identifies encryption
 * - Handles talkgroup management
 *
 * Supports both emergency services (380-400 MHz) and commercial TETRA
 */

// TETRA PDU types
enum class TETRAPDUType {
    SYSTEM_INFO,
    CALL_GRANT,
    CALL_RELEASE,
    REGISTRATION,
    AUTHENTICATION,
    SHORT_DATA,
    STATUS_UPDATE,
    LOCATION_UPDATE,
    UNKNOWN
};

// TETRA call information
struct TETRACall {
    uint32_t call_id;
    TalkgroupID talkgroup;
    RadioID radio_id;
    Frequency frequency;
    CallType type;
    EncryptionType encryption;
    uint64_t timestamp;
    bool is_emergency;
    std::string location;  // If available from SDS
};

// TETRA system information
struct TETRASystem {
    uint16_t mcc;           // Mobile Country Code
    uint16_t mnc;           // Mobile Network Code
    uint8_t color_code;     // Color code (0-3 typically)
    uint16_t location_area; // Location area
    std::string network_name;
    std::vector<Frequency> control_channels;
    std::vector<Frequency> traffic_channels;
    bool emergency_services;  // True if 380-400 MHz band
};

class TETRADecoder : public BaseDecoder {
public:
    TETRADecoder();
    ~TETRADecoder() override = default;

    void initialize() override;
    void processSymbols(const float* symbols, size_t count) override;
    void reset() override;

    SystemType getSystemType() const override { return SystemType::TETRA; }
    bool isLocked() const override { return phy_layer_.isSynchronized(); }

    // Configuration
    void setExpectedMCC(uint16_t mcc) { expected_mcc_ = mcc; }
    void setExpectedMNC(uint16_t mnc) { expected_mnc_ = mnc; }
    void setColorCode(uint8_t cc) { expected_color_code_ = cc; }

    // System information
    TETRASystem getSystemInfo() const { return system_info_; }
    bool hasSystemInfo() const { return has_system_info_; }

    // Active calls
    std::vector<TETRACall> getActiveCalls() const;

    // Statistics
    float getSignalQuality() const { return phy_layer_.getSignalQuality(); }
    size_t getCallsDecoded() const { return calls_decoded_; }

#ifdef ENABLE_TETRA_DECRYPTION
    // Decryption control (requires legal authorization)
    bool isDecryptionEnabled() const { return decryption_enabled_; }
    void enableDecryption(bool enable);

    // Decryption statistics
    struct DecryptionStats {
        size_t tea1_calls_encountered;
        size_t tea1_calls_decrypted;
        size_t keys_recovered;
        size_t decryption_failures;
    };
    DecryptionStats getDecryptionStats() const { return decryption_stats_; }
#endif

private:
    // MAC layer processing
    void processBurst(const TETRABurst& burst);
    void processBSCH(const uint8_t* data, size_t length);
    void processBNCH(const uint8_t* data, size_t length);
    void processMCCH(const uint8_t* data, size_t length);
    void processTCH(const uint8_t* data, size_t length);

    // PDU parsing
    TETRAPDUType identifyPDU(const uint8_t* data, size_t length);
    void parseSystemInfo(const uint8_t* data, size_t length);
    void parseCallGrant(const uint8_t* data, size_t length);
    void parseCallRelease(const uint8_t* data, size_t length);
    void parseShortData(const uint8_t* data, size_t length);

    // Encryption detection
    EncryptionType detectEncryption(const uint8_t* data);

#ifdef ENABLE_TETRA_DECRYPTION
    // Real-time decryption
    bool decryptVoiceFrame(uint8_t* data, size_t length, uint32_t call_id);
    bool attemptKeyRecovery(const uint8_t* ciphertext, size_t length,
                           uint32_t network_id, uint32_t talkgroup);
#endif

    // Utility functions
    uint32_t extractBits(const uint8_t* data, size_t start, size_t count);
    std::string bitsToString(const uint8_t* data, size_t start, size_t length);

    // Physical layer
    TETRAPhysicalLayer phy_layer_;

    // System configuration
    uint16_t expected_mcc_;
    uint16_t expected_mnc_;
    uint8_t expected_color_code_;

    // Decoded system information
    TETRASystem system_info_;
    bool has_system_info_;

    // Call tracking
    std::map<uint32_t, TETRACall> active_calls_;
    size_t calls_decoded_;

    // Talkgroup filtering
    std::set<TalkgroupID> monitored_talkgroups_;

    // Statistics
    size_t encrypted_calls_;
    size_t clear_calls_;

#ifdef ENABLE_TETRA_DECRYPTION
    // Decryption support
    TETRACrypto crypto_;
    bool decryption_enabled_;
    bool decryption_authorized_;
    DecryptionStats decryption_stats_;

    // Key cache for active calls: call_id -> key
    std::map<uint32_t, uint32_t> active_call_keys_;
#endif
};

} // namespace European
} // namespace TrunkSDR

#endif // TETRA_DECODER_H
