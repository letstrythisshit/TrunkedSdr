#ifndef P25_DECODER_H
#define P25_DECODER_H

#include "base_decoder.h"
#include <array>
#include <deque>
#include <map>

namespace TrunkSDR {

// P25 NAC (Network Access Code) is 12 bits
constexpr uint16_t P25_NAC_MASK = 0x0FFF;

// P25 Frame Sync patterns
constexpr uint64_t P25_FRAME_SYNC_1 = 0x5575F5FF77FF;
constexpr uint64_t P25_FRAME_SYNC_2 = 0x5575F5FF77FF;

// P25 Data Unit IDs (DUID)
enum class P25DUID : uint8_t {
    HEADER_DATA_UNIT = 0x0,
    TERMINATOR_DATA_UNIT = 0x3,
    LOGICAL_LINK_DATA_UNIT_1 = 0x5,
    LOGICAL_LINK_DATA_UNIT_2 = 0xA,
    TRUNKING_SIGNALING_BLOCK = 0x7,
    PDU = 0xC,
    UNKNOWN = 0xF
};

// P25 Trunking opcodes
enum class P25Opcode : uint8_t {
    GROUP_VOICE_GRANT = 0x00,
    GROUP_VOICE_UPDATE = 0x02,
    UNIT_TO_UNIT_VOICE_GRANT = 0x04,
    TELEPHONE_INTERCONNECT_VOICE_GRANT = 0x05,
    UNIT_REGISTRATION_RESPONSE = 0x2C,
    UNIT_AUTHENTICATION_COMMAND = 0x2D,
    STATUS_UPDATE = 0x30,
    STATUS_QUERY = 0x31,
    MESSAGE_UPDATE = 0x32,
    CALL_ALERT = 0x33,
    RFSS_STATUS_BROADCAST = 0x38,
    NETWORK_STATUS_BROADCAST = 0x3A,
    ADJACENT_SITE_STATUS_BROADCAST = 0x3B,
    IDENTIFIER_UPDATE = 0x3C
};

class P25Decoder : public BaseDecoder {
public:
    P25Decoder();
    ~P25Decoder() override = default;

    void initialize() override;
    void processSymbols(const float* symbols, size_t count) override;
    void reset() override;

    SystemType getSystemType() const override { return SystemType::P25_PHASE1; }
    bool isLocked() const override { return sync_locked_; }

    void setNAC(uint16_t nac) { expected_nac_ = nac; }
    uint16_t getNAC() const { return current_nac_; }

private:
    // Frame sync detection
    bool detectFrameSync(const std::deque<uint8_t>& bit_buffer);
    uint64_t bitsToU64(const std::deque<uint8_t>& bits, size_t start, size_t count);

    // NID (Network ID) processing
    bool processNID(const uint8_t* bits);
    uint16_t extractNAC(const uint8_t* bits);
    P25DUID extractDUID(const uint8_t* bits);

    // Trunking signaling
    void processTSBK(const uint8_t* bits, size_t length);
    void processGroupVoiceGrant(const uint8_t* data);
    void processIdentifierUpdate(const uint8_t* data);

    // Error correction
    bool checkAndCorrectGolay(uint8_t* data, size_t length);
    bool checkAndCorrectHamming(uint8_t* data, size_t length);

    // Utility functions
    uint32_t bitsToUint32(const uint8_t* bits, size_t start, size_t count);
    void uint32ToBits(uint32_t value, uint8_t* bits, size_t count);

    // State
    bool sync_locked_;
    uint16_t expected_nac_;
    uint16_t current_nac_;
    uint16_t wacn_;
    uint16_t system_id_;

    std::deque<uint8_t> bit_buffer_;
    std::vector<uint8_t> frame_buffer_;

    // Frame sync detector
    size_t sync_errors_;
    size_t sync_threshold_;

    // Frequency table for identifier updates
    std::map<uint8_t, Frequency> frequency_table_;

    // Statistics
    size_t frames_decoded_;
    size_t errors_corrected_;
};

} // namespace TrunkSDR

#endif // P25_DECODER_H
