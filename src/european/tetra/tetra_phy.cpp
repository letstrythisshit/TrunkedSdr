#include "tetra_phy.h"
#include "../../utils/logger.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace TrunkSDR {
namespace European {

// Convolutional encoder polynomials for TETRA (rate 2/3, constraint length 5)
constexpr uint8_t TETRA_CONV_POLY_1 = 0x1B;  // 11011
constexpr uint8_t TETRA_CONV_POLY_2 = 0x1D;  // 11101

// CRC polynomial for TETRA (CRC-16-CCITT)
constexpr uint16_t TETRA_CRC_POLY = 0x1021;

TETRAPhysicalLayer::TETRAPhysicalLayer()
    : sync_locked_(false),
      bits_since_sync_(0),
      current_frame_(0),
      current_multiframe_(0),
      current_slot_(0),
      sync_threshold_(3),
      sync_errors_allowed_(3),
      frames_without_sync_(0),
      signal_quality_(0.0f),
      bursts_decoded_(0),
      crc_errors_(0),
      avg_ber_(0.0f) {
}

void TETRAPhysicalLayer::initialize() {
    // Initialize Viterbi decoder with 2^(K-1) states for K=5
    size_t num_states = 1 << 4;  // 16 states
    viterbi_states_.resize(num_states);

    for (auto& state : viterbi_states_) {
        state.path_metric = 0;
        state.path.clear();
    }

    deinterleave_buffer_.resize(TETRA_BITS_PER_SLOT);

    Logger::instance().info("TETRA Physical Layer initialized");
    reset();
}

void TETRAPhysicalLayer::reset() {
    sync_locked_ = false;
    bit_buffer_.clear();
    bits_since_sync_ = 0;
    current_frame_ = 0;
    current_multiframe_ = 0;
    current_slot_ = 0;
    frames_without_sync_ = 0;
    burst_queue_.clear();

    for (auto& state : viterbi_states_) {
        state.path_metric = 0;
        state.path.clear();
    }
}

void TETRAPhysicalLayer::processSymbols(const float* symbols, size_t count) {
    // Convert symbols to bits (hard decision)
    for (size_t i = 0; i < count; i++) {
        uint8_t bit = (symbols[i] >= 2.0f) ? 1 : 0;
        bit_buffer_.push_back(bit);
    }

    // Maintain reasonable buffer size
    while (bit_buffer_.size() > TETRA_FRAME_BITS * 2) {
        bit_buffer_.pop_front();
    }

    // Try to find/maintain synchronization
    if (!sync_locked_) {
        if (bit_buffer_.size() >= 64) {  // Need enough bits for training sequence
            if (detectTrainingSequence()) {
                sync_locked_ = true;
                frames_without_sync_ = 0;
                Logger::instance().info("TETRA sync acquired");
            }
        }
    } else {
        bits_since_sync_++;

        // Check for sync every slot
        if (bits_since_sync_ >= TETRA_BITS_PER_SLOT) {
            bits_since_sync_ = 0;

            // Process the slot
            processSlot(current_slot_);

            current_slot_ = (current_slot_ + 1) % TETRA_SLOTS_PER_FRAME;
            if (current_slot_ == 0) {
                current_frame_++;
                if (current_frame_ >= 18) {  // 18 frames per multiframe
                    current_frame_ = 0;
                    current_multiframe_++;
                }
            }

            // Verify sync is still valid
            if (!detectTrainingSequence()) {
                frames_without_sync_++;
                if (frames_without_sync_ > 10) {
                    sync_locked_ = false;
                    Logger::instance().warning("TETRA sync lost");
                }
            } else {
                frames_without_sync_ = 0;
            }
        }
    }
}

bool TETRAPhysicalLayer::detectTrainingSequence() {
    if (bit_buffer_.size() < 64) {
        return false;
    }

    // Training sequence is typically at specific positions in the burst
    // For TETRA, it's usually in the middle of the burst
    // Try to detect it within a window

    size_t best_pos = 0;
    size_t best_distance = 100;
    TETRABurstType detected_type = TETRABurstType::UNKNOWN;

    for (size_t pos = 0; pos < std::min(bit_buffer_.size() - 11, size_t(64)); pos++) {
        // Extract 11 bits for training sequence
        uint16_t seq = 0;
        for (size_t i = 0; i < 11; i++) {
            seq = (seq << 1) | bit_buffer_[pos + i];
        }

        // Check against known training sequences
        size_t dist_normal = hammingDistance(seq, TETRA_TRAINING_SEQ_NORMAL, 11);
        size_t dist_ext = hammingDistance(seq, TETRA_TRAINING_SEQ_EXTENDED, 11);
        size_t dist_sync = hammingDistance(seq, TETRA_TRAINING_SEQ_SYNC, 11);

        size_t min_dist = std::min({dist_normal, dist_ext, dist_sync});

        if (min_dist < best_distance) {
            best_distance = min_dist;
            best_pos = pos;

            if (min_dist == dist_normal) {
                detected_type = TETRABurstType::NORMAL_UPLINK;
            } else if (min_dist == dist_sync) {
                detected_type = TETRABurstType::SYNCHRONIZATION;
            } else {
                detected_type = TETRABurstType::CONTROL_UPLINK;
            }
        }
    }

    // Accept sync if within threshold
    if (best_distance <= sync_errors_allowed_) {
        // Remove bits before training sequence
        for (size_t i = 0; i < best_pos; i++) {
            bit_buffer_.pop_front();
        }

        signal_quality_ = 1.0f - (static_cast<float>(best_distance) / 11.0f);
        return true;
    }

    return false;
}

size_t TETRAPhysicalLayer::hammingDistance(uint64_t a, uint64_t b, size_t bits) {
    uint64_t xor_val = a ^ b;
    size_t distance = 0;
    for (size_t i = 0; i < bits; i++) {
        if (xor_val & (1ULL << i)) {
            distance++;
        }
    }
    return distance;
}

TETRABurstType TETRAPhysicalLayer::identifyBurstType(uint16_t training_seq) {
    size_t dist_normal = hammingDistance(training_seq, TETRA_TRAINING_SEQ_NORMAL, 11);
    size_t dist_ext = hammingDistance(training_seq, TETRA_TRAINING_SEQ_EXTENDED, 11);
    size_t dist_sync = hammingDistance(training_seq, TETRA_TRAINING_SEQ_SYNC, 11);

    size_t min_dist = std::min({dist_normal, dist_ext, dist_sync});

    if (min_dist == dist_normal) {
        return TETRABurstType::NORMAL_UPLINK;
    } else if (min_dist == dist_sync) {
        return TETRABurstType::SYNCHRONIZATION;
    } else {
        return TETRABurstType::CONTROL_UPLINK;
    }
}

void TETRAPhysicalLayer::processSlot(uint8_t slot_num) {
    if (bit_buffer_.size() < TETRA_BITS_PER_SLOT) {
        return;
    }

    TETRABurst burst;
    burst.slot_number = slot_num;
    burst.frame_number = current_frame_;
    burst.multiframe_number = current_multiframe_;
    burst.crc_valid = false;
    burst.ber = 0.0f;

    // Extract slot bits
    std::vector<uint8_t> slot_bits(TETRA_BITS_PER_SLOT);
    extractBits(bit_buffer_, slot_bits.data(), 0, TETRA_BITS_PER_SLOT);

    // Deinterleave
    deinterleave(slot_bits.data(), deinterleave_buffer_.data(), TETRA_BITS_PER_SLOT);

    // Descramble
    descramble(deinterleave_buffer_.data(), TETRA_BITS_PER_SLOT, current_frame_);

    // Viterbi decode (rate 2/3 convolutional code)
    std::vector<uint8_t> decoded_bits(TETRA_BITS_PER_SLOT * 2 / 3);
    if (viterbiDecode(deinterleave_buffer_.data(), decoded_bits.data(),
                      decoded_bits.size())) {

        // CRC check
        burst.crc_valid = checkCRC16(decoded_bits.data(), decoded_bits.size());

        if (!burst.crc_valid) {
            crc_errors_++;
        }

        // Reed-Muller decoding for certain fields (if applicable)
        // This depends on the logical channel type

        burst.bits = decoded_bits;
        burst.type = TETRABurstType::NORMAL_DOWNLINK;
        burst.channel = TETRALogicalChannel::MCCH;  // Will be refined by MAC layer

        burst_queue_.push_back(burst);
        bursts_decoded_++;
    }

    // Remove processed bits
    for (size_t i = 0; i < TETRA_BITS_PER_SLOT; i++) {
        if (!bit_buffer_.empty()) {
            bit_buffer_.pop_front();
        }
    }
}

void TETRAPhysicalLayer::deinterleave(const uint8_t* input, uint8_t* output,
                                       size_t length) {
    // TETRA uses block interleaving
    // Simplified rectangular interleaver (actual TETRA uses more complex scheme)
    size_t rows = 30;
    size_t cols = length / rows;

    for (size_t i = 0; i < length; i++) {
        size_t row = i / cols;
        size_t col = i % cols;
        size_t interleaved_pos = col * rows + row;

        if (interleaved_pos < length) {
            output[i] = input[interleaved_pos];
        } else {
            output[i] = 0;
        }
    }
}

bool TETRAPhysicalLayer::viterbiDecode(const uint8_t* input, uint8_t* output,
                                        size_t output_length) {
    // Simplified Viterbi decoder for TETRA convolutional code
    // Rate 2/3, constraint length K=5

    size_t num_states = viterbi_states_.size();
    size_t input_length = output_length * 3 / 2;

    // Initialize all path metrics to high value except state 0
    for (size_t i = 0; i < num_states; i++) {
        viterbi_states_[i].path_metric = (i == 0) ? 0 : 1000000;
        viterbi_states_[i].path.clear();
    }

    // Forward pass through trellis
    for (size_t t = 0; t < output_length; t++) {
        std::vector<ViterbiState> new_states(num_states);

        for (size_t i = 0; i < num_states; i++) {
            new_states[i].path_metric = 1000000;
        }

        for (size_t state = 0; state < num_states; state++) {
            // For each possible input bit (0 or 1)
            for (uint8_t bit = 0; bit <= 1; bit++) {
                // Compute next state
                uint8_t next_state = ((state << 1) | bit) & 0x0F;

                // Compute expected output (simplified - would use generator polynomials)
                // This is a placeholder - actual implementation would be more complex
                uint8_t expected_0 = (state ^ bit) & 1;
                uint8_t expected_1 = ((state >> 1) ^ bit) & 1;

                // Hamming distance to received bits
                size_t idx = t * 3 / 2;
                if (idx + 1 < input_length) {
                    size_t dist = (expected_0 != input[idx]) + (expected_1 != input[idx + 1]);

                    uint64_t new_metric = viterbi_states_[state].path_metric + dist;

                    if (new_metric < new_states[next_state].path_metric) {
                        new_states[next_state].path_metric = new_metric;
                        new_states[next_state].path = viterbi_states_[state].path;
                        new_states[next_state].path.push_back(bit);
                    }
                }
            }
        }

        viterbi_states_ = new_states;
    }

    // Find best final state
    size_t best_state = 0;
    uint64_t best_metric = viterbi_states_[0].path_metric;
    for (size_t i = 1; i < num_states; i++) {
        if (viterbi_states_[i].path_metric < best_metric) {
            best_metric = viterbi_states_[i].path_metric;
            best_state = i;
        }
    }

    // Output decoded bits
    for (size_t i = 0; i < std::min(output_length, viterbi_states_[best_state].path.size()); i++) {
        output[i] = viterbi_states_[best_state].path[i];
    }

    // Calculate BER estimate
    avg_ber_ = static_cast<float>(best_metric) / static_cast<float>(output_length * 2);

    return true;
}

bool TETRAPhysicalLayer::reedMullerDecode(uint8_t* data, size_t length) {
    // Simplified Reed-Muller decoder
    // TETRA uses RM(16,7) for some control information
    // This is a placeholder - full implementation would be more complex
    return true;
}

bool TETRAPhysicalLayer::checkCRC16(const uint8_t* data, size_t length) {
    if (length < 16) {
        return false;
    }

    // Extract CRC from last 16 bits
    uint16_t received_crc = bitsToUint32(data, length - 16, 16);

    // Calculate CRC on data (excluding CRC field)
    uint16_t calculated_crc = calculateCRC16(data, length - 16);

    return (received_crc == calculated_crc);
}

uint16_t TETRAPhysicalLayer::calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;  // Initial value

    for (size_t i = 0; i < length; i++) {
        if (data[i]) {
            crc ^= (1 << 15);
        }

        for (int j = 0; j < 1; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ TETRA_CRC_POLY;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

void TETRAPhysicalLayer::descramble(uint8_t* data, size_t length, uint32_t frame_num) {
    // TETRA scrambling sequence based on frame number
    // Simplified implementation - actual TETRA uses a specific LFSR
    uint32_t lfsr = 0x1FF ^ frame_num;

    for (size_t i = 0; i < length; i++) {
        // Generate scrambling bit
        uint8_t scram_bit = (lfsr ^ (lfsr >> 5)) & 1;

        // XOR with data
        data[i] ^= scram_bit;

        // Update LFSR
        lfsr = ((lfsr << 1) | scram_bit) & 0x1FF;
    }
}

void TETRAPhysicalLayer::extractBits(const std::deque<uint8_t>& source,
                                      uint8_t* dest, size_t start, size_t count) {
    for (size_t i = 0; i < count && (start + i) < source.size(); i++) {
        dest[i] = source[start + i];
    }
}

uint32_t TETRAPhysicalLayer::bitsToUint32(const uint8_t* bits, size_t start,
                                           size_t count) {
    uint32_t value = 0;
    for (size_t i = 0; i < count && i < 32; i++) {
        value = (value << 1) | (bits[start + i] & 1);
    }
    return value;
}

bool TETRAPhysicalLayer::hasBurst() const {
    return !burst_queue_.empty();
}

TETRABurst TETRAPhysicalLayer::getBurst() {
    TETRABurst burst;
    if (!burst_queue_.empty()) {
        burst = burst_queue_.front();
        burst_queue_.pop_front();
    }
    return burst;
}

} // namespace European
} // namespace TrunkSDR
