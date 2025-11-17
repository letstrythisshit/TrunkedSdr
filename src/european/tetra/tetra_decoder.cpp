#include "tetra_decoder.h"
#include "../../utils/logger.h"
#include <algorithm>
#include <cstring>
#include <sstream>

namespace TrunkSDR {
namespace European {

// TETRA MAC PDU identifiers
constexpr uint8_t TETRA_MAC_RESOURCE = 0x00;
constexpr uint8_t TETRA_MAC_BROADCAST = 0x01;
constexpr uint8_t TETRA_MAC_D_SETUP = 0x02;
constexpr uint8_t TETRA_MAC_D_CONNECT = 0x03;
constexpr uint8_t TETRA_MAC_D_RELEASE = 0x04;
constexpr uint8_t TETRA_MAC_D_SDS = 0x05;

TETRADecoder::TETRADecoder()
    : expected_mcc_(0),
      expected_mnc_(0),
      expected_color_code_(0),
      has_system_info_(false),
      calls_decoded_(0),
      encrypted_calls_(0),
      clear_calls_(0)
#ifdef ENABLE_TETRA_DECRYPTION
      , decryption_enabled_(false),
      decryption_authorized_(false)
#endif
{
#ifdef ENABLE_TETRA_DECRYPTION
    decryption_stats_ = {0, 0, 0, 0};
#endif
}

void TETRADecoder::initialize() {
    phy_layer_.initialize();

    system_info_ = TETRASystem{};
    system_info_.emergency_services = false;

    Logger::instance().info("TETRA Decoder initialized");
    reset();
}

void TETRADecoder::reset() {
    phy_layer_.reset();
    active_calls_.clear();
    has_system_info_ = false;
    calls_decoded_ = 0;
    encrypted_calls_ = 0;
    clear_calls_ = 0;
}

void TETRADecoder::processSymbols(const float* symbols, size_t count) {
    // Feed symbols to physical layer
    phy_layer_.processSymbols(symbols, count);

    // Process any decoded bursts
    while (phy_layer_.hasBurst()) {
        TETRABurst burst = phy_layer_.getBurst();
        if (burst.crc_valid) {
            processBurst(burst);
        }
    }
}

void TETRADecoder::processBurst(const TETRABurst& burst) {
    // Route burst to appropriate handler based on logical channel
    switch (burst.channel) {
        case TETRALogicalChannel::BSCH:
            processBSCH(burst.bits.data(), burst.bits.size());
            break;

        case TETRALogicalChannel::BNCH:
            processBNCH(burst.bits.data(), burst.bits.size());
            break;

        case TETRALogicalChannel::MCCH:
        case TETRALogicalChannel::AACH:
        case TETRALogicalChannel::SCH_F:
        case TETRALogicalChannel::SCH_HD:
            processMCCH(burst.bits.data(), burst.bits.size());
            break;

        case TETRALogicalChannel::TCH:
        case TETRALogicalChannel::STCH:
            processTCH(burst.bits.data(), burst.bits.size());
            break;

        default:
            Logger::instance().debug("Unknown TETRA logical channel");
            break;
    }
}

void TETRADecoder::processBSCH(const uint8_t* data, size_t length) {
    // Broadcast Synchronization Channel - contains basic system info
    if (length < 60) {
        return;
    }

    // Extract MCC (10 bits, starting at bit 0)
    uint16_t mcc = static_cast<uint16_t>(extractBits(data, 0, 10));

    // Extract MNC (14 bits, starting at bit 10)
    uint16_t mnc = static_cast<uint16_t>(extractBits(data, 10, 14));

    // Extract color code (6 bits, starting at bit 24)
    uint8_t cc = static_cast<uint8_t>(extractBits(data, 24, 6));

    system_info_.mcc = mcc;
    system_info_.mnc = mnc;
    system_info_.color_code = cc & 0x03;  // Usually only 2 bits used

    // Determine if emergency services based on MCC/MNC
    // This is a simplified check - full implementation would use lookup table
    system_info_.emergency_services = (mcc >= 200 && mcc <= 799);

    has_system_info_ = true;

    Logger::instance().info("TETRA System: MCC=%u, MNC=%u, CC=%u, Emergency=%s",
                 mcc, mnc, system_info_.color_code,
                 system_info_.emergency_services ? "YES" : "NO");

    // Notify via callback
    if (system_info_callback_) {
        SystemInfo info;
        info.type = SystemType::TETRA;
        info.system_id = (mcc << 16) | mnc;
        info.name = "TETRA System";
        system_info_callback_(info);
    }
}

void TETRADecoder::processBNCH(const uint8_t* data, size_t length) {
    // Broadcast Network Channel - contains network parameters
    if (length < 80) {
        return;
    }

    // Extract location area (16 bits)
    uint16_t location_area = static_cast<uint16_t>(extractBits(data, 0, 16));
    system_info_.location_area = location_area;

    // Extract network name if present (simplified)
    // Full implementation would decode text according to TETRA character set
    std::string network_name = bitsToString(data, 32, std::min(size_t(64), length - 32));
    if (!network_name.empty()) {
        system_info_.network_name = network_name;
        Logger::instance().info("TETRA Network: %s (LA=%u)", network_name.c_str(), location_area);
    }
}

void TETRADecoder::processMCCH(const uint8_t* data, size_t length) {
    // Main Control Channel - contains call grants and control messages
    if (length < 16) {
        return;
    }

    TETRAPDUType pdu_type = identifyPDU(data, length);

    switch (pdu_type) {
        case TETRAPDUType::SYSTEM_INFO:
            parseSystemInfo(data, length);
            break;

        case TETRAPDUType::CALL_GRANT:
            parseCallGrant(data, length);
            break;

        case TETRAPDUType::CALL_RELEASE:
            parseCallRelease(data, length);
            break;

        case TETRAPDUType::SHORT_DATA:
            parseShortData(data, length);
            break;

        default:
            Logger::instance().debug("Unknown TETRA PDU type");
            break;
    }
}

void TETRADecoder::processTCH(const uint8_t* data, size_t length) {
    // Traffic Channel - voice or data
    // Check for stealing channel (STCH) which carries short data in voice bursts

    if (length < 10) {
        return;
    }

    // Check stealing bits (bit 0 and bit 114 typically)
    bool stealing_flag = (data[0] == 1);

    if (stealing_flag) {
        // STCH - parse short data
        parseShortData(data, length);
    } else {
        // Regular voice frame
        // Check if this voice frame is encrypted
        EncryptionType encryption = detectEncryption(data);

#ifdef ENABLE_TETRA_DECRYPTION
        if (encryption == EncryptionType::TEA1 && decryption_enabled_ && decryption_authorized_) {
            // Attempt real-time decryption
            // Create mutable copy for decryption
            std::vector<uint8_t> mutable_data(data, data + length);

            // Find the call ID for this traffic channel (simplified - would track from grants)
            uint32_t call_id = 0; // Would be determined from channel/slot tracking

            if (decryptVoiceFrame(mutable_data.data(), mutable_data.size(), call_id)) {
                Logger::instance().info("✓ TETRA voice frame decrypted in real-time");
                // Pass decrypted data to codec decoder
                // In full implementation: invoke ACELP decoder with mutable_data
                decryption_stats_.tea1_calls_decrypted++;
            } else {
                Logger::instance().warning("✗ TETRA voice frame decryption failed");
                decryption_stats_.decryption_failures++;
            }
        } else if (encryption != EncryptionType::NONE) {
            if (encryption == EncryptionType::TEA1) {
                Logger::instance().debug("TETRA voice frame: TEA1 encrypted (decryption not enabled)");
            } else {
                Logger::instance().debug("TETRA voice frame: Encrypted with secure algorithm (TEA2/3/4)");
            }
        } else {
            Logger::instance().debug("TETRA voice frame: Clear (not encrypted)");
            // Pass clear voice to codec decoder
            // In full implementation: invoke ACELP decoder
        }
#else
        // Decryption not compiled in
        if (encryption != EncryptionType::NONE) {
            Logger::instance().debug("TETRA voice frame: Encrypted (decryption not available)");
        } else {
            Logger::instance().debug("TETRA voice frame: Clear");
            // Pass to codec decoder
        }
#endif
    }
}

TETRAPDUType TETRADecoder::identifyPDU(const uint8_t* data, size_t length) {
    if (length < 8) {
        return TETRAPDUType::UNKNOWN;
    }

    // PDU type is in first 8 bits (MAC PDU type)
    uint8_t pdu_id = static_cast<uint8_t>(extractBits(data, 0, 8));

    switch (pdu_id) {
        case TETRA_MAC_BROADCAST:
            return TETRAPDUType::SYSTEM_INFO;
        case TETRA_MAC_D_SETUP:
            return TETRAPDUType::CALL_GRANT;
        case TETRA_MAC_D_RELEASE:
            return TETRAPDUType::CALL_RELEASE;
        case TETRA_MAC_D_SDS:
            return TETRAPDUType::SHORT_DATA;
        default:
            return TETRAPDUType::UNKNOWN;
    }
}

void TETRADecoder::parseSystemInfo(const uint8_t* data, size_t length) {
    // Additional system information from MAC layer
    // This supplements what's in BSCH/BNCH
    Logger::instance().debug("TETRA system info PDU");
}

void TETRADecoder::parseCallGrant(const uint8_t* data, size_t length) {
    if (length < 80) {
        return;
    }

    TETRACall call;

    // Extract call type (4 bits at offset 8)
    uint8_t call_type_bits = static_cast<uint8_t>(extractBits(data, 8, 4));

    if (call_type_bits == 0) {
        call.type = CallType::GROUP;
    } else if (call_type_bits == 1) {
        call.type = CallType::PRIVATE;
    } else if (call_type_bits == 4) {
        call.type = CallType::EMERGENCY;
        call.is_emergency = true;
    } else {
        call.type = CallType::UNKNOWN;
    }

    // Extract talkgroup/destination (24 bits at offset 12)
    call.talkgroup = extractBits(data, 12, 24);

    // Extract source radio ID (24 bits at offset 36)
    call.radio_id = extractBits(data, 36, 24);

    // Extract frequency information (12 bits at offset 60)
    uint32_t freq_index = extractBits(data, 60, 12);

    // Calculate actual frequency (simplified - depends on band and base freq)
    // For 380-400 MHz emergency band:
    double base_freq = 380000000.0;  // 380 MHz
    call.frequency = base_freq + (freq_index * TETRA_CHANNEL_SPACING);

    // Detect encryption (2 bits at offset 72)
    call.encryption = detectEncryption(data + 9);

    call.timestamp = 0;  // Would use actual timestamp
    call.call_id = calls_decoded_;

    // Store active call
    active_calls_[call.call_id] = call;
    calls_decoded_++;

    if (call.encryption != EncryptionType::NONE) {
        encrypted_calls_++;

#ifdef ENABLE_TETRA_DECRYPTION
        if (call.encryption == EncryptionType::TEA1) {
            decryption_stats_.tea1_calls_encountered++;
            Logger::instance().warning("TETRA Call Grant: TG=%u, Source=%u, Freq=%.4f MHz [TEA1 ENCRYPTED - VULNERABLE]",
                           call.talkgroup, call.radio_id, call.frequency / 1e6);

            // If decryption is enabled and authorized, prepare for key recovery
            if (decryption_enabled_ && decryption_authorized_) {
                Logger::instance().info("  → Will attempt key recovery when traffic begins");
            } else {
                Logger::instance().info("  → Decryption not enabled (use --enable-decryption)");
            }
        } else {
            Logger::instance().warning("TETRA Call Grant: TG=%u, Source=%u, Freq=%.4f MHz [%s ENCRYPTED - SECURE]",
                           call.talkgroup, call.radio_id, call.frequency / 1e6,
                           (call.encryption == EncryptionType::TEA2 ? "TEA2" :
                            call.encryption == EncryptionType::TEA3 ? "TEA3" : "TEA4"));
        }
#else
        Logger::instance().warning("TETRA Call Grant: TG=%u, Source=%u, Freq=%.4f MHz [ENCRYPTED %d]",
                       call.talkgroup, call.radio_id, call.frequency / 1e6,
                       static_cast<int>(call.encryption));
#endif
    } else {
        clear_calls_++;
        Logger::instance().info("TETRA Call Grant: TG=%u, Source=%u, Freq=%.4f MHz [CLEAR]",
                    call.talkgroup, call.radio_id, call.frequency / 1e6);
    }

    // Notify via callback
    if (grant_callback_) {
        CallGrant grant;
        grant.talkgroup = call.talkgroup;
        grant.radio_id = call.radio_id;
        grant.frequency = call.frequency;
        grant.type = call.type;
        grant.encrypted = (call.encryption != EncryptionType::NONE);
        grant.priority = call.is_emergency ? 10 : 5;
        grant.timestamp = call.timestamp;
        grant_callback_(grant);
    }
}

void TETRADecoder::parseCallRelease(const uint8_t* data, size_t length) {
    if (length < 32) {
        return;
    }

    // Extract call ID (24 bits)
    uint32_t call_id = extractBits(data, 8, 24);

    // Remove from active calls
    auto it = active_calls_.find(call_id);
    if (it != active_calls_.end()) {
        Logger::instance().info("TETRA Call Release: TG=%u", it->second.talkgroup);
        active_calls_.erase(it);
    }
}

void TETRADecoder::parseShortData(const uint8_t* data, size_t length) {
    // Short Data Service (SDS) - text messages, status, GPS, etc.
    if (length < 32) {
        return;
    }

    // SDS type (4 bits)
    uint8_t sds_type = static_cast<uint8_t>(extractBits(data, 8, 4));

    // Extract text data (simplified - would decode properly)
    std::string sds_text = bitsToString(data, 32, std::min(size_t(128), length - 32));

    if (!sds_text.empty()) {
        Logger::instance().info("TETRA SDS: %s", sds_text.c_str());
    }
}

EncryptionType TETRADecoder::detectEncryption(const uint8_t* data) {
    // Encryption bits indicate encryption type
    // Bit 0-1: encryption class
    uint8_t enc_bits = static_cast<uint8_t>(extractBits(data, 0, 2));

    switch (enc_bits) {
        case 0:
            return EncryptionType::NONE;
        case 1:
            return EncryptionType::TEA1;
        case 2:
            return EncryptionType::TEA2;
        case 3:
            // Check additional bits for TEA3/TEA4
            uint8_t enc_ext = static_cast<uint8_t>(extractBits(data, 2, 2));
            if (enc_ext == 0) {
                return EncryptionType::TEA3;
            } else {
                return EncryptionType::TEA4;
            }
    }

    return EncryptionType::UNKNOWN_ENCRYPTED;
}

uint32_t TETRADecoder::extractBits(const uint8_t* data, size_t start, size_t count) {
    uint32_t value = 0;
    for (size_t i = 0; i < count && i < 32; i++) {
        size_t bit_idx = start + i;
        size_t byte_idx = bit_idx / 8;
        size_t bit_in_byte = 7 - (bit_idx % 8);

        uint8_t bit = (data[byte_idx] >> bit_in_byte) & 1;
        value = (value << 1) | bit;
    }
    return value;
}

std::string TETRADecoder::bitsToString(const uint8_t* data, size_t start, size_t length) {
    // Simplified text extraction - TETRA uses specific character encoding
    std::ostringstream oss;

    for (size_t i = 0; i < length / 8; i++) {
        uint8_t ch = static_cast<uint8_t>(extractBits(data, start + i * 8, 8));
        if (ch >= 32 && ch < 127) {  // Printable ASCII
            oss << static_cast<char>(ch);
        }
    }

    return oss.str();
}

std::vector<TETRACall> TETRADecoder::getActiveCalls() const {
    std::vector<TETRACall> calls;
    for (const auto& pair : active_calls_) {
        calls.push_back(pair.second);
    }
    return calls;
}

#ifdef ENABLE_TETRA_DECRYPTION
void TETRADecoder::enableDecryption(bool enable) {
    if (enable) {
        // Check legal authorization FIRST
        if (!TETRACryptoLegalChecker::checkAuthorization()) {
            Logger::instance().error("TETRA decryption authorization DENIED");
            Logger::instance().error("Legal acknowledgment required - see documentation");
            decryption_enabled_ = false;
            decryption_authorized_ = false;
            return;
        }

        Logger::instance().warning("⚠️  TETRA DECRYPTION ENABLED");
        Logger::instance().warning("⚠️  User has acknowledged legal responsibility");
        Logger::instance().warning("⚠️  Only TEA1 can be decrypted (CVE-2022-24402)");
        decryption_enabled_ = true;
        decryption_authorized_ = true;
    } else {
        Logger::instance().info("TETRA decryption disabled");
        decryption_enabled_ = false;
    }
}

bool TETRADecoder::decryptVoiceFrame(uint8_t* data, size_t length, uint32_t call_id) {
    // Check if we have a cached key for this call
    auto key_it = active_call_keys_.find(call_id);

    if (key_it == active_call_keys_.end()) {
        // No key cached - need to attempt key recovery
        // Get call information
        auto call_it = active_calls_.find(call_id);
        if (call_it == active_calls_.end()) {
            Logger::instance().error("Cannot decrypt: Unknown call ID %u", call_id);
            return false;
        }

        const TETRACall& call = call_it->second;
        uint32_t network_id = (system_info_.mcc << 16) | system_info_.mnc;

        // Attempt key recovery
        if (!attemptKeyRecovery(data, length, network_id, call.talkgroup)) {
            return false;
        }

        // Key should now be cached
        key_it = active_call_keys_.find(call_id);
        if (key_it == active_call_keys_.end()) {
            Logger::instance().error("Key recovery succeeded but key not cached (internal error)");
            return false;
        }
    }

    uint32_t key = key_it->second;

    // Decrypt the voice frame using cached key
    auto result = crypto_.decryptTEA1(data, length, key);

    if (result.success) {
        // Copy decrypted data back
        memcpy(data, result.plaintext.data(), std::min(length, result.plaintext.size()));
        return true;
    }

    return false;
}

bool TETRADecoder::attemptKeyRecovery(const uint8_t* ciphertext, size_t length,
                                      uint32_t network_id, uint32_t talkgroup) {
    Logger::instance().info("Attempting TEA1 key recovery (network=0x%08X, TG=%u)...",
                           network_id, talkgroup);
    Logger::instance().warning("This may take up to 90 seconds on Raspberry Pi");

    // Check if key is already known in crypto module's cache
    uint32_t cached_key;
    if (crypto_.hasKnownKey(network_id, talkgroup, &cached_key)) {
        Logger::instance().info("✓ Using cached key from previous recovery: 0x%08X", cached_key);

        // Find the active call for this talkgroup
        for (auto& call_pair : active_calls_) {
            if (call_pair.second.talkgroup == talkgroup) {
                active_call_keys_[call_pair.first] = cached_key;
                break;
            }
        }

        return true;
    }

    // Perform key recovery (this is CPU-intensive, optimized for ARM)
    auto key_result = crypto_.recoverTEA1Key(ciphertext, length, nullptr, 0);

    if (key_result.success) {
        Logger::instance().info("✓ Key recovered successfully!");
        Logger::instance().info("  Key: 0x%08X", key_result.recovered_key);
        Logger::instance().info("  Time: %.2f seconds", key_result.time_seconds);
        Logger::instance().info("  Attempts: %llu", (unsigned long long)key_result.attempts);

        // Cache the key for this network/talkgroup
        crypto_.addKnownKey(network_id, talkgroup, key_result.recovered_key);

        // Cache the key for active calls on this talkgroup
        for (auto& call_pair : active_calls_) {
            if (call_pair.second.talkgroup == talkgroup) {
                active_call_keys_[call_pair.first] = key_result.recovered_key;
            }
        }

        decryption_stats_.keys_recovered++;
        return true;
    } else {
        Logger::instance().error("✗ Key recovery failed: %s", key_result.error_message.c_str());
        return false;
    }
}
#endif // ENABLE_TETRA_DECRYPTION

} // namespace European
} // namespace TrunkSDR
