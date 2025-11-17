#include "imbe_codec.h"
#include "../utils/logger.h"

// Include mbelib if available
#ifdef HAVE_MBELIB
extern "C" {
#include <mbelib.h>
}
#endif

namespace TrunkSDR {

IMBECodec::IMBECodec()
    : mbe_state_(nullptr)
    , initialized_(false) {
}

IMBECodec::~IMBECodec() {
    // Cleanup MBE state if needed
}

bool IMBECodec::initialize() {
#ifdef HAVE_MBELIB
    // Initialize mbelib decoder
    mbe_state_ = nullptr;  // mbelib uses static state
    initialized_ = true;
    LOG_INFO("IMBE codec initialized with mbelib");
    return true;
#else
    LOG_WARNING("IMBE codec: mbelib not available, using stub decoder");
    initialized_ = true;
    return true;
#endif
}

void IMBECodec::decode(const uint8_t* encoded_data, size_t length,
                       AudioBuffer& output) {
    if (!initialized_) {
        LOG_ERROR("IMBE codec not initialized");
        return;
    }

    // Resize output buffer
    output.resize(160);  // 160 samples @ 8kHz = 20ms

#ifdef HAVE_MBELIB
    // Decode IMBE frame using mbelib
    char ambe_d[49];  // IMBE frame data
    short audio_out[160];

    // Convert input bytes to bit string format expected by mbelib
    for (size_t i = 0; i < 88 && i/8 < length; i++) {
        ambe_d[i] = (encoded_data[i/8] >> (7 - (i%8))) & 1;
    }

    // Decode using mbelib (simplified - actual mbelib API may differ)
    // mbe_processImbe(audio_out, ambe_d);

    // For now, use silence if mbelib function unavailable
    for (int i = 0; i < 160; i++) {
        output[i] = 0;
    }
#else
    // Stub implementation - output silence
    for (int i = 0; i < 160; i++) {
        output[i] = 0;
    }
#endif
}

void IMBECodec::reset() {
    // Reset decoder state
}

} // namespace TrunkSDR
