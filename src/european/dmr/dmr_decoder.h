#ifndef DMR_DECODER_H
#define DMR_DECODER_H

#include "../../decoders/base_decoder.h"
#include <map>
#include <deque>

namespace TrunkSDR {
namespace European {

/**
 * DMR (Digital Mobile Radio) Tier II/III Decoder
 *
 * Supports:
 * - DMR Tier II (conventional repeater mode)
 * - DMR Tier III (trunking: Capacity Plus, Connect Plus)
 * - 2-slot TDMA
 * - CSBK (Control Signaling Block) decoding
 * - Color Code system (0-15)
 * - AMBE+2 voice codec support
 * - Talker Alias decoding
 *
 * Common in European commercial/utilities
 */

// DMR constants
constexpr size_t DMR_FRAME_BITS = 264;      // Total bits in DMR frame
constexpr size_t DMR_SYNC_PATTERN_BITS = 48;
constexpr size_t DMR_SLOT_TYPE_BITS = 20;
constexpr size_t DMR_INFO_BITS = 196;
constexpr size_t DMR_SLOTS_PER_FRAME = 2;
constexpr float DMR_FRAME_DURATION_MS = 30.0f;
constexpr float DMR_SLOT_DURATION_MS = 15.0f;

// DMR sync patterns
constexpr uint64_t DMR_SYNC_BS_SOURCED = 0x755FD7DF75F7;  // Base station
constexpr uint64_t DMR_SYNC_MS_SOURCED = 0xDFF57D75DF5D;  // Mobile station
constexpr uint64_t DMR_SYNC_DATA = 0xD5D7F77FD757;         // Data
constexpr uint64_t DMR_SYNC_VOICE = 0x7F7D5DD57DFD;        // Voice

// DMR Data types
enum class DMRDataType {
    VOICE_LC_HEADER,    // Voice with Link Control header
    VOICE_TERMINATOR,   // Voice terminator with LC
    CSBK,               // Control Signaling Block
    DATA_HEADER,        // Data header
    RATE_1_2_DATA,      // Rate 1/2 data
    RATE_3_4_DATA,      // Rate 3/4 data
    IDLE,               // Idle burst
    UNKNOWN
};

// CSBK (Control Signaling Block) opcodes
enum class DMRCSBKOpcode {
    UNIT_TO_UNIT_VOICE_SERVICE_REQUEST = 0x04,
    UNIT_TO_UNIT_VOICE_SERVICE_ANSWER = 0x05,
    CHANNEL_GRANT = 0x06,
    MOVE = 0x07,
    BROADCAST_TALKGROUP_ANNOUNCE = 0x08,
    NEGATIVE_ACKNOWLEDGE = 0x26,
    PREAMBLE = 0x3D,
    UNKNOWN = 0xFF
};

// DMR call structure
struct DMRCall {
    uint32_t source_id;
    uint32_t destination_id;
    CallType type;  // GROUP, PRIVATE
    uint8_t color_code;
    uint8_t slot_number;  // 1 or 2
    Frequency frequency;
    uint64_t timestamp;
    bool group_call;
    bool emergency;
    std::string talker_alias;
};

// DMR Capacity Plus trunking types
enum class DMRTrunkingType {
    NONE,               // Tier II conventional
    CAPACITY_PLUS,      // Single-site trunking
    CAPACITY_PLUS_MULTI,// Multi-site Capacity Plus
    CONNECT_PLUS,       // Wide-area trunking
    HYTERA_XPT,         // Hytera pseudo-trunking
    LINKED_CAPACITY     // Linked Capacity Plus
};

class DMRDecoder : public BaseDecoder {
public:
    DMRDecoder();
    ~DMRDecoder() override = default;

    void initialize() override;
    void processSymbols(const float* symbols, size_t count) override;
    void reset() override;

    SystemType getSystemType() const override { return SystemType::DMR_TIER3; }
    bool isLocked() const override { return sync_locked_; }

    // Configuration
    void setColorCode(uint8_t cc) { expected_color_code_ = cc; }
    void setTrunkingType(DMRTrunkingType type) { trunking_type_ = type; }
    void setRestChannel(Frequency freq) { rest_channel_freq_ = freq; }

    // Statistics
    uint8_t getColorCode() const { return detected_color_code_; }
    size_t getCallsDecoded() const { return calls_decoded_; }

private:
    // Synchronization
    bool detectSync();
    size_t hammingDistance64(uint64_t a, uint64_t b);
    uint64_t bitsToU64(const std::deque<uint8_t>& bits, size_t start, size_t count);

    // Frame processing
    void processSlot(uint8_t slot_num, const uint8_t* data);
    DMRDataType decodeSlotType(const uint8_t* slot_type_bits);
    uint8_t extractColorCode(const uint8_t* slot_type_bits);

    // CSBK processing (Capacity Plus trunking)
    void processCSBK(const uint8_t* data, size_t length);
    DMRCSBKOpcode extractCSBKOpcode(const uint8_t* data);
    void parseChannelGrant(const uint8_t* data);
    void parseTalkgroupAnnounce(const uint8_t* data);

    // Voice LC (Link Control) processing
    void processVoiceLC(const uint8_t* data, size_t length);
    void parseTalkerAlias(const uint8_t* data);

    // Error correction
    void golay_24_12_decode(uint8_t* data);
    void hamming_16_11_decode(uint8_t* data);
    bool bptc_196_96_decode(const uint8_t* input, uint8_t* output);

    // Deinterleaving
    void deinterleave(const uint8_t* input, uint8_t* output, size_t length);

    // Utility functions
    uint32_t bitsToUint32(const uint8_t* bits, size_t start, size_t count);

    // State
    bool sync_locked_;
    std::deque<uint8_t> bit_buffer_;
    size_t bits_since_sync_;

    // DMR configuration
    uint8_t expected_color_code_;
    uint8_t detected_color_code_;
    DMRTrunkingType trunking_type_;
    Frequency rest_channel_freq_;

    // Slot tracking
    uint8_t current_slot_;
    bool slot_active_[2];

    // Call tracking
    std::map<uint32_t, DMRCall> active_calls_;
    size_t calls_decoded_;

    // Talker alias reconstruction (sent over multiple frames)
    std::map<uint32_t, std::vector<uint8_t>> talker_alias_fragments_;
};

} // namespace European
} // namespace TrunkSDR

#endif // DMR_DECODER_H
