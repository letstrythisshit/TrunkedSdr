#ifndef CODEC_INTERFACE_H
#define CODEC_INTERFACE_H

#include "../utils/types.h"
#include <vector>

namespace TrunkSDR {

class CodecInterface {
public:
    virtual ~CodecInterface() = default;

    virtual bool initialize() = 0;
    virtual void decode(const uint8_t* encoded_data, size_t length,
                       AudioBuffer& output) = 0;
    virtual void reset() = 0;

    virtual CodecType getType() const = 0;
    virtual size_t getFrameSize() const = 0;  // Bytes per encoded frame
    virtual size_t getOutputSamples() const = 0;  // Samples per decoded frame
};

} // namespace TrunkSDR

#endif // CODEC_INTERFACE_H
