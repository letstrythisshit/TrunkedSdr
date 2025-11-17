#ifndef TETRA_PHY_H
#define TETRA_PHY_H

#include "../../utils/types.h"
#include <vector>
#include <deque>
#include <cstdint>

namespace TrunkSDR {
namespace European {

/**
 * TETRA Physical Layer Decoder
 *
 * Handles:
 * - Synchronization pattern detection (training sequences)
 * - Frame synchronization
 * - Slot detection (4-slot TDMA)
 * - Deinterleaving
 * - Convolutional decoding (rate 2/3, K=5)
 * - Reed-Muller error correction
 * - CRC checking
 */

// TETRA frame structure constants
constexpr size_t TETRA_SLOTS_PER_FRAME = 4;
constexpr size_t TETRA_BITS_PER_SLOT = 510;
constexpr size_t TETRA_FRAME_BITS = 2040;  // 4 slots * 510 bits
constexpr float TETRA_FRAME_DURATION_MS = 14.167f;  // milliseconds
constexpr float TETRA_SLOT_DURATION_MS = 3.542f;    // milliseconds

// Training sequence (synchronization pattern) - 11 bits
constexpr uint16_t TETRA_TRAINING_SEQ_NORMAL = 0x0FD;      // Normal uplink
constexpr uint16_t TETRA_TRAINING_SEQ_EXTENDED = 0x6E4;    // Extended uplink
constexpr uint16_t TETRA_TRAINING_SEQ_SYNC = 0x3AA;        // Synchronization burst

// Burst types
enum class TETRABurstType {
    NORMAL_UPLINK,
    NORMAL_DOWNLINK,
    CONTROL_UPLINK,
    CONTROL_DOWNLINK,
    SYNCHRONIZATION,
    LINEARIZATION,
    UNKNOWN
};

// Logical channel types
enum class TETRALogicalChannel {
    BSCH,   // Broadcast Synchronization Channel
    BNCH,   // Broadcast Network Channel
    MCCH,   // Main Control Channel
    TCH,    // Traffic Channel (voice/data)
    STCH,   // Stealing Channel (short data in TCH)
    AACH,   // Access Assignment Channel
    SCH_F,  // Signaling Channel Full slot
    SCH_HD, // Signaling Channel Half slot Downlink
    SCH_HU, // Signaling Channel Half slot Uplink
    UNKNOWN
};

// Decoded burst structure
struct TETRABurst {
    TETRABurstType type;
    TETRALogicalChannel channel;
    uint8_t slot_number;      // 0-3
    uint32_t frame_number;
    uint32_t multiframe_number;
    std::vector<uint8_t> bits;  // Decoded and error-corrected bits
    bool crc_valid;
    float ber;                  // Bit Error Rate estimate
};

class TETRAPhysicalLayer {
public:
    TETRAPhysicalLayer();
    ~TETRAPhysicalLayer() = default;

    void initialize();
    void reset();

    // Process symbols from demodulator
    void processSymbols(const float* symbols, size_t count);

    // Check if synchronized to TETRA signal
    bool isSynchronized() const { return sync_locked_; }

    // Get decoded bursts
    bool hasBurst() const;
    TETRABurst getBurst();

    // Statistics
    float getSignalQuality() const { return signal_quality_; }
    size_t getBurstsDecoded() const { return bursts_decoded_; }

private:
    // Synchronization
    bool detectTrainingSequence();
    size_t hammingDistance(uint64_t a, uint64_t b, size_t bits);
    TETRABurstType identifyBurstType(uint16_t training_seq);

    // Frame processing
    void processSlot(uint8_t slot_num);
    void deinterleave(const uint8_t* input, uint8_t* output, size_t length);

    // Error correction
    bool viterbiDecode(const uint8_t* input, uint8_t* output, size_t length);
    bool reedMullerDecode(uint8_t* data, size_t length);
    bool checkCRC16(const uint8_t* data, size_t length);
    uint16_t calculateCRC16(const uint8_t* data, size_t length);

    // Scrambling/descrambling
    void descramble(uint8_t* data, size_t length, uint32_t frame_num);

    // Utility functions
    void extractBits(const std::deque<uint8_t>& source, uint8_t* dest,
                     size_t start, size_t count);
    uint32_t bitsToUint32(const uint8_t* bits, size_t start, size_t count);

    // State
    bool sync_locked_;
    std::deque<uint8_t> bit_buffer_;
    size_t bits_since_sync_;

    // Frame tracking
    uint32_t current_frame_;
    uint32_t current_multiframe_;
    uint8_t current_slot_;

    // Synchronization thresholds
    size_t sync_threshold_;
    size_t sync_errors_allowed_;
    size_t frames_without_sync_;

    // Viterbi decoder state
    struct ViterbiState {
        uint64_t path_metric;
        std::vector<uint8_t> path;
    };
    std::vector<ViterbiState> viterbi_states_;

    // Deinterleaving matrix
    std::vector<uint8_t> deinterleave_buffer_;

    // Output queue
    std::deque<TETRABurst> burst_queue_;

    // Statistics
    float signal_quality_;
    size_t bursts_decoded_;
    size_t crc_errors_;
    float avg_ber_;
};

} // namespace European
} // namespace TrunkSDR

#endif // TETRA_PHY_H
