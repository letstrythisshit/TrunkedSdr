#ifndef IMBE_CODEC_H
#define IMBE_CODEC_H

#include "codec_interface.h"

// Forward declare mbelib structures (compatible with mbelib.h)
extern "C" {
    struct mbe_parameters;
}

namespace TrunkSDR {

// IMBE codec for P25 Phase 1
class IMBECodec : public CodecInterface {
public:
    IMBECodec();
    ~IMBECodec() override;

    bool initialize() override;
    void decode(const uint8_t* encoded_data, size_t length,
               AudioBuffer& output) override;
    void reset() override;

    CodecType getType() const override { return CodecType::IMBE; }
    size_t getFrameSize() const override { return 11; }  // 88 bits
    size_t getOutputSamples() const override { return 160; }  // 20ms at 8kHz

private:
    void* mbe_state_;  // Opaque pointer to MBE decoder state
    bool initialized_;
};

} // namespace TrunkSDR

#endif // IMBE_CODEC_H
