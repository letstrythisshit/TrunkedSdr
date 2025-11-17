#include "dmr_decoder.h"
#include "../../utils/logger.h"
#include <algorithm>
#include <cstring>

namespace TrunkSDR {
namespace European {

DMRDecoder::DMRDecoder()
    : sync_locked_(false),
      bits_since_sync_(0),
      expected_color_code_(1),
      detected_color_code_(0),
      trunking_type_(DMRTrunkingType::CAPACITY_PLUS),
      rest_channel_freq_(0.0),
      current_slot_(0),
      calls_decoded_(0) {

    slot_active_[0] = false;
    slot_active_[1] = false;
}

void DMRDecoder::initialize() {
    Logger::instance().info("DMR Decoder initialized (Tier III / Capacity Plus)");
    reset();
}

void DMRDecoder::reset() {
    sync_locked_ = false;
    bit_buffer_.clear();
    bits_since_sync_ = 0;
    current_slot_ = 0;
    active_calls_.clear();
    talker_alias_fragments_.clear();
    calls_decoded_ = 0;
    slot_active_[0] = false;
    slot_active_[1] = false;
}

void DMRDecoder::processSymbols(const float* symbols, size_t count) {
    // Convert symbols (0-3) to dibits (2 bits each)
    for (size_t i = 0; i < count; i++) {
        int sym = static_cast<int>(symbols[i]);
        uint8_t bit0 = (sym >> 1) & 1;
        uint8_t bit1 = sym & 1;

        bit_buffer_.push_back(bit0);
        bit_buffer_.push_back(bit1);
    }

    // Maintain reasonable buffer size
    while (bit_buffer_.size() > DMR_FRAME_BITS * 4) {
        bit_buffer_.pop_front();
    }

    // Try to find/maintain synchronization
    if (!sync_locked_) {
        if (bit_buffer_.size() >= DMR_SYNC_PATTERN_BITS) {
            if (detectSync()) {
                sync_locked_ = true;
                Logger::instance().info("DMR sync acquired");
            }
        }
    } else {
        bits_since_sync_++;

        // Process frame every 264 bits
        if (bits_since_sync_ >= DMR_FRAME_BITS) {
            bits_since_sync_ = 0;

            // Extract slot data (after sync pattern)
            if (bit_buffer_.size() >= DMR_FRAME_BITS) {
                std::vector<uint8_t> slot_data(DMR_FRAME_BITS);
                for (size_t i = 0; i < DMR_FRAME_BITS && i < bit_buffer_.size(); i++) {
                    slot_data[i] = bit_buffer_[i];
                }

                processSlot(current_slot_, slot_data.data());

                // Toggle slot (TDMA)
                current_slot_ = (current_slot_ == 0) ? 1 : 0;

                // Remove processed bits
                for (size_t i = 0; i < DMR_FRAME_BITS; i++) {
                    if (!bit_buffer_.empty()) {
                        bit_buffer_.pop_front();
                    }
                }
            }

            // Verify sync is still valid
            if (!detectSync()) {
                sync_locked_ = false;
                Logger::instance().warning("DMR sync lost");
            }
        }
    }
}

bool DMRDecoder::detectSync() {
    if (bit_buffer_.size() < DMR_SYNC_PATTERN_BITS) {
        return false;
    }

    // Try to detect sync pattern
    uint64_t received_sync = bitsToU64(bit_buffer_, 0, DMR_SYNC_PATTERN_BITS);

    size_t dist_bs = hammingDistance64(received_sync, DMR_SYNC_BS_SOURCED);
    size_t dist_ms = hammingDistance64(received_sync, DMR_SYNC_MS_SOURCED);
    size_t dist_data = hammingDistance64(received_sync, DMR_SYNC_DATA);
    size_t dist_voice = hammingDistance64(received_sync, DMR_SYNC_VOICE);

    size_t min_dist = std::min({dist_bs, dist_ms, dist_data, dist_voice});

    // Accept if within threshold (4 bit errors)
    if (min_dist <= 4) {
        // Remove bits before sync
        while (bit_buffer_.size() > 0 && bit_buffer_[0] != ((received_sync >> 47) & 1)) {
            bit_buffer_.pop_front();
        }
        return true;
    }

    return false;
}

size_t DMRDecoder::hammingDistance64(uint64_t a, uint64_t b) {
    uint64_t xor_val = a ^ b;
    size_t distance = 0;
    while (xor_val) {
        distance += xor_val & 1;
        xor_val >>= 1;
    }
    return distance;
}

uint64_t DMRDecoder::bitsToU64(const std::deque<uint8_t>& bits, size_t start, size_t count) {
    uint64_t value = 0;
    for (size_t i = 0; i < count && i < 64 && (start + i) < bits.size(); i++) {
        value = (value << 1) | (bits[start + i] & 1);
    }
    return value;
}

void DMRDecoder::processSlot(uint8_t slot_num, const uint8_t* data) {
    // Skip sync pattern (first 48 bits)
    const uint8_t* slot_type_ptr = data + 48;

    // Decode slot type (20 bits after sync)
    uint8_t slot_type_bits[20];
    std::memcpy(slot_type_bits, slot_type_ptr, 20);

    // Extract color code from slot type
    detected_color_code_ = extractColorCode(slot_type_bits);

    // Check color code matches
    if (detected_color_code_ != expected_color_code_) {
        Logger::instance().debug("DMR color code mismatch: expected=%u, got=%u",
                     expected_color_code_, detected_color_code_);
        return;
    }

    // Decode slot type
    DMRDataType data_type = decodeSlotType(slot_type_bits);

    // Extract info bits (196 bits)
    const uint8_t* info_bits = data + 48 + 20;

    switch (data_type) {
        case DMRDataType::CSBK:
            processCSBK(info_bits, DMR_INFO_BITS);
            break;

        case DMRDataType::VOICE_LC_HEADER:
            processVoiceLC(info_bits, DMR_INFO_BITS);
            break;

        case DMRDataType::VOICE_TERMINATOR:
            Logger::instance().debug("DMR voice terminator on slot %u", slot_num);
            slot_active_[slot_num] = false;
            break;

        default:
            Logger::instance().debug("DMR data type %d on slot %u", static_cast<int>(data_type), slot_num);
            break;
    }
}

DMRDataType DMRDecoder::decodeSlotType(const uint8_t* slot_type_bits) {
    // Slot type is 10 bits (after Golay error correction)
    uint8_t slot_type_data[10];
    std::memcpy(slot_type_data, slot_type_bits, 10);

    // Apply Golay(20,10) error correction
    // Simplified - full implementation would decode properly
    uint8_t data_type_bits = (slot_type_data[0] << 3) | (slot_type_data[1] << 2) |
                             (slot_type_data[2] << 1) | slot_type_data[3];

    switch (data_type_bits) {
        case 0x00: return DMRDataType::VOICE_LC_HEADER;
        case 0x01: return DMRDataType::VOICE_TERMINATOR;
        case 0x03: return DMRDataType::CSBK;
        case 0x06: return DMRDataType::DATA_HEADER;
        case 0x09: return DMRDataType::IDLE;
        default: return DMRDataType::UNKNOWN;
    }
}

uint8_t DMRDecoder::extractColorCode(const uint8_t* slot_type_bits) {
    // Color code is 4 bits embedded in slot type
    // Simplified extraction
    return (slot_type_bits[4] << 3) | (slot_type_bits[5] << 2) |
           (slot_type_bits[6] << 1) | slot_type_bits[7];
}

void DMRDecoder::processCSBK(const uint8_t* data, size_t length) {
    // Control Signaling Block - used for Capacity Plus trunking

    // Decode BPTC (196,96) error correction
    std::vector<uint8_t> decoded(96);
    if (!bptc_196_96_decode(data, decoded.data())) {
        Logger::instance().debug("DMR CSBK decode failed");
        return;
    }

    // Extract CSBK opcode (6 bits)
    DMRCSBKOpcode opcode = extractCSBKOpcode(decoded.data());

    switch (opcode) {
        case DMRCSBKOpcode::CHANNEL_GRANT:
            parseChannelGrant(decoded.data());
            break;

        case DMRCSBKOpcode::BROADCAST_TALKGROUP_ANNOUNCE:
            parseTalkgroupAnnounce(decoded.data());
            break;

        case DMRCSBKOpcode::PREAMBLE:
            Logger::instance().debug("DMR Capacity Plus preamble");
            break;

        default:
            Logger::instance().debug("DMR CSBK opcode: 0x%02X", static_cast<uint8_t>(opcode));
            break;
    }
}

DMRCSBKOpcode DMRDecoder::extractCSBKOpcode(const uint8_t* data) {
    uint8_t opcode = bitsToUint32(data, 0, 6) & 0x3F;
    return static_cast<DMRCSBKOpcode>(opcode);
}

void DMRDecoder::parseChannelGrant(const uint8_t* data) {
    // Extract source and destination IDs
    uint32_t source_id = bitsToUint32(data, 16, 24);
    uint32_t dest_id = bitsToUint32(data, 40, 24);

    // Extract logical channel (for Capacity Plus)
    uint8_t logical_slot = bitsToUint32(data, 8, 1) & 1;

    DMRCall call;
    call.source_id = source_id;
    call.destination_id = dest_id;
    call.slot_number = logical_slot;
    call.color_code = detected_color_code_;
    call.frequency = rest_channel_freq_;  // Would calculate actual frequency
    call.group_call = true;  // Usually group calls
    call.type = CallType::GROUP;
    call.timestamp = 0;  // Would use actual timestamp

    active_calls_[dest_id] = call;
    calls_decoded_++;

    Logger::instance().info("DMR Channel Grant: Slot=%u, TG=%u, Source=%u, CC=%u",
                 logical_slot, dest_id, source_id, detected_color_code_);

    // Notify via callback
    if (grant_callback_) {
        CallGrant grant;
        grant.talkgroup = dest_id;
        grant.radio_id = source_id;
        grant.frequency = call.frequency;
        grant.type = CallType::GROUP;
        grant.encrypted = false;  // DMR encryption detection would go here
        grant.priority = 5;
        grant.timestamp = 0;
        grant_callback_(grant);
    }
}

void DMRDecoder::parseTalkgroupAnnounce(const uint8_t* data) {
    // Capacity Plus talkgroup announcement
    uint32_t talkgroup = bitsToUint32(data, 16, 24);
    Logger::instance().info("DMR Talkgroup Announce: TG=%u", talkgroup);
}

void DMRDecoder::processVoiceLC(const uint8_t* data, size_t length) {
    // Voice with Link Control header

    // Decode BPTC (196,96)
    std::vector<uint8_t> decoded(96);
    if (!bptc_196_96_decode(data, decoded.data())) {
        return;
    }

    // Extract source and destination from LC
    uint32_t source_id = bitsToUint32(decoded.data(), 16, 24);
    uint32_t dest_id = bitsToUint32(decoded.data(), 40, 24);

    Logger::instance().info("DMR Voice LC: TG=%u, Source=%u", dest_id, source_id);

    // Check for talker alias blocks (sent in subsequent frames)
    parseTalkerAlias(decoded.data());
}

void DMRDecoder::parseTalkerAlias(const uint8_t* data) {
    // Talker Alias is sent as text in multiple fragments
    // This is a simplified version - full implementation would reassemble fragments

    // Extract alias bytes (7 bytes typically)
    std::string alias;
    for (size_t i = 0; i < 7; i++) {
        uint8_t ch = bitsToUint32(data, 64 + i * 8, 8);
        if (ch >= 32 && ch < 127) {
            alias += static_cast<char>(ch);
        }
    }

    if (!alias.empty()) {
        Logger::instance().info("DMR Talker Alias: %s", alias.c_str());
    }
}

bool DMRDecoder::bptc_196_96_decode(const uint8_t* input, uint8_t* output) {
    // Block Product Turbo Code (196,96) used in DMR
    // Simplified implementation - full version would do proper turbo decoding

    // For now, extract raw data bits (would apply error correction)
    size_t out_idx = 0;
    for (size_t i = 0; i < 196 && out_idx < 96; i++) {
        // Skip check bits, keep data bits (simplified pattern)
        if ((i % 15) < 11) {
            output[out_idx++] = input[i];
        }
    }

    return true;
}

uint32_t DMRDecoder::bitsToUint32(const uint8_t* bits, size_t start, size_t count) {
    uint32_t value = 0;
    for (size_t i = 0; i < count && i < 32; i++) {
        value = (value << 1) | (bits[start + i] & 1);
    }
    return value;
}

} // namespace European
} // namespace TrunkSDR
