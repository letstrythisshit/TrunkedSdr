#include "smartnet_decoder.h"
#include "../utils/logger.h"
#include <cstring>

namespace TrunkSDR {

SmartNetDecoder::SmartNetDecoder()
    : sync_locked_(false)
    , baud_rate_(3600)
    , base_frequency_(851000000)  // Default 851 MHz
    , channel_spacing_(25000)     // 25 kHz spacing
    , frames_decoded_(0)
    , sync_errors_(0)
    , sync_threshold_(5) {
}

void SmartNetDecoder::initialize() {
    LOG_INFO("SmartNet decoder initialized, baud rate =", baud_rate_);
    reset();
}

void SmartNetDecoder::reset() {
    sync_locked_ = false;
    bit_buffer_.clear();
    frames_decoded_ = 0;
    sync_errors_ = 0;
}

void SmartNetDecoder::processSymbols(const float* symbols, size_t count) {
    for (size_t i = 0; i < count; i++) {
        // SmartNet uses FSK2 (binary)
        uint8_t bit = (symbols[i] > 0.5f) ? 1 : 0;
        bit_buffer_.push_back(bit);

        // Keep buffer manageable
        while (bit_buffer_.size() > 5000) {
            bit_buffer_.pop_front();
        }

        // Look for sync
        if (!sync_locked_ || sync_errors_ > sync_threshold_) {
            if (detectSync(bit_buffer_)) {
                sync_locked_ = true;
                sync_errors_ = 0;
                LOG_INFO("SmartNet sync acquired");
            }
        }

        // Process frames when locked
        if (sync_locked_ && bit_buffer_.size() >= SMARTNET_FRAME_BITS) {
            std::vector<uint8_t> frame_bits(SMARTNET_FRAME_BITS);
            for (size_t j = 0; j < SMARTNET_FRAME_BITS; j++) {
                frame_bits[j] = bit_buffer_[j];
            }

            if (processFrame(frame_bits.data())) {
                frames_decoded_++;
                bit_buffer_.erase(bit_buffer_.begin(),
                                 bit_buffer_.begin() + SMARTNET_FRAME_BITS);
            } else {
                sync_errors_++;
                bit_buffer_.pop_front();
            }
        }
    }
}

bool SmartNetDecoder::detectSync(const std::deque<uint8_t>& bit_buffer) {
    if (bit_buffer.size() < 16) return false;

    // Check for SmartNet sync pattern (16 bits)
    uint16_t sync_word = 0;
    for (int i = 0; i < 16; i++) {
        sync_word = (sync_word << 1) | (bit_buffer[i] & 1);
    }

    // Allow for 2 bit errors
    uint16_t xor_result = sync_word ^ SMARTNET_SYNC;
    int bit_errors = __builtin_popcount(xor_result);

    return bit_errors <= 2;
}

bool SmartNetDecoder::processFrame(const uint8_t* bits) {
    // SmartNet OSW (Outbound Signaling Word) format:
    // Sync(16) | Address(10) | Group(3) | Command(11) | CRC(16) | Status(20)

    // Skip sync (already detected)
    uint16_t address = bitsToUint16(bits, 16, 10);
    uint16_t group = bitsToUint16(bits, 26, 3);
    uint16_t command = bitsToUint16(bits, 29, 11);

    // Check CRC
    if (!checkCRC(bits)) {
        return false;
    }

    // Decode OSW
    decodeOSW(address, group, command);

    return true;
}

void SmartNetDecoder::decodeOSW(uint16_t address, uint16_t group, uint16_t command) {
    LOG_DEBUG("SmartNet OSW: Address =", address, "Group =", group,
              "Command =", std::hex, command);

    // Decode command type
    uint16_t cmd_type = (command >> 6) & 0x1F;

    if (cmd_type == 0x00) {  // Group Call
        uint16_t channel = command & 0x3F;

        // Calculate frequency
        Frequency frequency = base_frequency_ + (channel * channel_spacing_);

        LOG_INFO("SmartNet Group Call: TG =", address, "Channel =", channel,
                 "Freq =", frequency);

        if (grant_callback_) {
            CallGrant grant;
            grant.talkgroup = address;
            grant.radio_id = 0;  // Not provided in SmartNet
            grant.frequency = frequency;
            grant.type = CallType::GROUP;
            grant.priority = 5;
            grant.timestamp = std::time(nullptr);
            grant.encrypted = false;

            grant_callback_(grant);
        }
    }
}

uint16_t SmartNetDecoder::crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++) {
        crc ^= (data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

bool SmartNetDecoder::checkCRC(const uint8_t* frame) {
    // Simplified CRC check
    // In production, implement full SmartNet CRC-16
    return true;  // Placeholder
}

uint16_t SmartNetDecoder::bitsToUint16(const uint8_t* bits, size_t start, size_t count) {
    uint16_t result = 0;
    for (size_t i = 0; i < count && i < 16; i++) {
        result = (result << 1) | (bits[start + i] & 1);
    }
    return result;
}

} // namespace TrunkSDR
