#ifndef SMARTNET_DECODER_H
#define SMARTNET_DECODER_H

#include "base_decoder.h"
#include <deque>
#include <map>

namespace TrunkSDR {

// SmartNet frame is 76 bits (38 dibits)
constexpr size_t SMARTNET_FRAME_BITS = 76;

// SmartNet sync patterns
constexpr uint32_t SMARTNET_SYNC = 0x5555;

// SmartNet command types
enum class SmartNetCommand : uint8_t {
    IDLE = 0x2F0,
    GROUP_CALL = 0x300,
    PRIVATE_CALL = 0x308,
    STATUS = 0x310,
    AFFILIATION = 0x318,
    UNKNOWN = 0xFFF
};

class SmartNetDecoder : public BaseDecoder {
public:
    SmartNetDecoder();
    ~SmartNetDecoder() override = default;

    void initialize() override;
    void processSymbols(const float* symbols, size_t count) override;
    void reset() override;

    SystemType getSystemType() const override { return SystemType::SMARTNET; }
    bool isLocked() const override { return sync_locked_; }

    void setBaudRate(uint32_t baud_rate) { baud_rate_ = baud_rate; }

private:
    bool detectSync(const std::deque<uint8_t>& bit_buffer);
    bool processFrame(const uint8_t* bits);
    void decodeOSW(uint16_t address, uint16_t group, uint16_t command);

    uint16_t crc16(const uint8_t* data, size_t length);
    bool checkCRC(const uint8_t* frame);

    uint16_t bitsToUint16(const uint8_t* bits, size_t start, size_t count);

    bool sync_locked_;
    uint32_t baud_rate_;  // 3600 or 9600

    std::deque<uint8_t> bit_buffer_;

    // Band plan for frequency calculation
    Frequency base_frequency_;
    Frequency channel_spacing_;
    std::map<uint16_t, Frequency> channel_map_;

    size_t frames_decoded_;
    size_t sync_errors_;
    size_t sync_threshold_;
};

} // namespace TrunkSDR

#endif // SMARTNET_DECODER_H
