#ifndef BASE_DECODER_H
#define BASE_DECODER_H

#include "../utils/types.h"
#include <functional>
#include <vector>

namespace TrunkSDR {

// Callback for call grants (voice channel assignments)
using GrantCallback = std::function<void(const CallGrant&)>;

// Callback for system information updates
using SystemInfoCallback = std::function<void(const SystemInfo&)>;

class BaseDecoder {
public:
    virtual ~BaseDecoder() = default;

    virtual void initialize() = 0;
    virtual void processSymbols(const float* symbols, size_t count) = 0;
    virtual void reset() = 0;

    virtual SystemType getSystemType() const = 0;
    virtual bool isLocked() const = 0;

    void setGrantCallback(GrantCallback callback) {
        grant_callback_ = callback;
    }

    void setSystemInfoCallback(SystemInfoCallback callback) {
        system_info_callback_ = callback;
    }

protected:
    GrantCallback grant_callback_;
    SystemInfoCallback system_info_callback_;
};

} // namespace TrunkSDR

#endif // BASE_DECODER_H
