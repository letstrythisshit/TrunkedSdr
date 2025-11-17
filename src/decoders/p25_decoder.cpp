#include "p25_decoder.h"
#include "../utils/logger.h"
#include <cstring>
#include <cmath>

namespace TrunkSDR {

P25Decoder::P25Decoder()
    : sync_locked_(false)
    , expected_nac_(0)
    , current_nac_(0)
    , wacn_(0)
    , system_id_(0)
    , sync_errors_(0)
    , sync_threshold_(3)
    , frames_decoded_(0)
    , errors_corrected_(0) {
}

void P25Decoder::initialize() {
    LOG_INFO("P25 decoder initialized");
    reset();
}

void P25Decoder::reset() {
    sync_locked_ = false;
    bit_buffer_.clear();
    frame_buffer_.clear();
    sync_errors_ = 0;
    frames_decoded_ = 0;
    errors_corrected_ = 0;
}

void P25Decoder::processSymbols(const float* symbols, size_t count) {
    for (size_t i = 0; i < count; i++) {
        // Convert C4FM symbol (0, 1, 2, 3) to dibits
        int symbol = static_cast<int>(symbols[i]);

        // Extract two bits from symbol
        uint8_t bit1 = (symbol >> 1) & 1;
        uint8_t bit2 = symbol & 1;

        bit_buffer_.push_back(bit1);
        bit_buffer_.push_back(bit2);

        // Keep buffer size manageable
        while (bit_buffer_.size() > 10000) {
            bit_buffer_.pop_front();
        }

        // Try to find frame sync
        if (!sync_locked_ || sync_errors_ > sync_threshold_) {
            if (detectFrameSync(bit_buffer_)) {
                sync_locked_ = true;
                sync_errors_ = 0;
                LOG_INFO("P25 frame sync acquired");
            }
        }

        // Process frames when locked
        if (sync_locked_ && bit_buffer_.size() >= 1728) {  // Full P25 frame
            // Extract NID (64 bits after sync)
            uint8_t nid_bits[64];
            for (int j = 0; j < 64; j++) {
                nid_bits[j] = bit_buffer_[48 + j];
            }

            if (processNID(nid_bits)) {
                // Process based on DUID
                P25DUID duid = extractDUID(nid_bits);

                if (duid == P25DUID::TRUNKING_SIGNALING_BLOCK) {
                    // Extract TSBK data
                    std::vector<uint8_t> tsbk_data(144);  // 144 bits
                    for (size_t j = 0; j < 144; j++) {
                        tsbk_data[j] = bit_buffer_[112 + j];
                    }
                    processTSBK(tsbk_data.data(), 144);
                }

                frames_decoded_++;
                bit_buffer_.erase(bit_buffer_.begin(), bit_buffer_.begin() + 1728);
            } else {
                sync_errors_++;
                bit_buffer_.pop_front();
            }
        }
    }
}

bool P25Decoder::detectFrameSync(const std::deque<uint8_t>& bit_buffer) {
    if (bit_buffer.size() < 48) return false;

    // Check for P25 frame sync pattern
    uint64_t sync_pattern = bitsToU64(bit_buffer, 0, 48);

    // Allow for some bit errors in sync
    uint64_t xor_result = sync_pattern ^ P25_FRAME_SYNC_1;
    int bit_errors = __builtin_popcountll(xor_result);

    return bit_errors <= 4;  // Allow up to 4 bit errors
}

uint64_t P25Decoder::bitsToU64(const std::deque<uint8_t>& bits, size_t start, size_t count) {
    uint64_t result = 0;
    for (size_t i = 0; i < count && (start + i) < bits.size(); i++) {
        result = (result << 1) | (bits[start + i] & 1);
    }
    return result;
}

bool P25Decoder::processNID(const uint8_t* bits) {
    // Extract NAC (12 bits)
    current_nac_ = extractNAC(bits);

    // Check NAC if configured
    if (expected_nac_ != 0 && current_nac_ != expected_nac_) {
        return false;
    }

    return true;
}

uint16_t P25Decoder::extractNAC(const uint8_t* bits) {
    uint16_t nac = 0;
    for (int i = 0; i < 12; i++) {
        nac = (nac << 1) | (bits[i] & 1);
    }
    return nac & P25_NAC_MASK;
}

P25DUID P25Decoder::extractDUID(const uint8_t* bits) {
    // DUID is 4 bits at position 60-63
    uint8_t duid = 0;
    for (int i = 0; i < 4; i++) {
        duid = (duid << 1) | (bits[60 + i] & 1);
    }
    return static_cast<P25DUID>(duid);
}

void P25Decoder::processTSBK(const uint8_t* bits, size_t length) {
    // Extract opcode (6 bits)
    uint8_t opcode = bitsToUint32(bits, 0, 6);

    LOG_DEBUG("P25 TSBK opcode:", std::hex, static_cast<int>(opcode));

    switch (static_cast<P25Opcode>(opcode)) {
        case P25Opcode::GROUP_VOICE_GRANT:
        case P25Opcode::GROUP_VOICE_UPDATE:
            processGroupVoiceGrant(bits);
            break;

        case P25Opcode::IDENTIFIER_UPDATE:
            processIdentifierUpdate(bits);
            break;

        default:
            LOG_DEBUG("Unhandled P25 opcode:", std::hex, static_cast<int>(opcode));
            break;
    }
}

void P25Decoder::processGroupVoiceGrant(const uint8_t* data) {
    // Extract fields from Group Voice Grant
    // Format: Opcode(6) | Options(8) | Service(8) | Frequency(12) | Group(16) | Source(24)

    uint8_t options = bitsToUint32(data, 6, 8);
    uint16_t freq_id = bitsToUint32(data, 22, 12);
    uint16_t talkgroup = bitsToUint32(data, 34, 16);
    uint32_t source = bitsToUint32(data, 50, 24);

    // Look up frequency from identifier table
    Frequency frequency = 0;
    if (frequency_table_.count(freq_id & 0xFF)) {
        frequency = frequency_table_[freq_id & 0xFF];
    }

    LOG_INFO("P25 Voice Grant: TG =", talkgroup, "Source =", source, "Freq ID =", freq_id);

    // Create call grant
    if (grant_callback_ && frequency > 0) {
        CallGrant grant;
        grant.talkgroup = talkgroup;
        grant.radio_id = source;
        grant.frequency = frequency;
        grant.type = CallType::GROUP;
        grant.priority = 5;  // Default priority
        grant.timestamp = std::time(nullptr);
        grant.encrypted = (options & 0x40) != 0;

        grant_callback_(grant);
    }
}

void P25Decoder::processIdentifierUpdate(const uint8_t* data) {
    // Identifier Update provides frequency table
    // Format varies, simplified implementation

    uint8_t identifier = bitsToUint32(data, 6, 4);
    uint32_t base_freq = bitsToUint32(data, 10, 32);
    uint16_t spacing = bitsToUint32(data, 42, 10);
    uint16_t offset = bitsToUint32(data, 52, 10);

    // Convert to actual frequency (Hz)
    // P25 uses 5 kHz channel spacing by default
    Frequency freq = base_freq * 5000.0;

    frequency_table_[identifier] = freq;

    LOG_DEBUG("P25 Identifier Update: ID =", static_cast<int>(identifier),
              "Freq =", freq, "Hz");
}

uint32_t P25Decoder::bitsToUint32(const uint8_t* bits, size_t start, size_t count) {
    uint32_t result = 0;
    for (size_t i = 0; i < count && i < 32; i++) {
        result = (result << 1) | (bits[start + i] & 1);
    }
    return result;
}

} // namespace TrunkSDR
