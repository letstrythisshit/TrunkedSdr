#ifndef SDR_INTERFACE_H
#define SDR_INTERFACE_H

#include "../utils/types.h"
#include <functional>
#include <atomic>
#include <thread>

namespace TrunkSDR {

// Callback type for I/Q samples
using SampleCallback = std::function<void(const Complex*, size_t)>;

class SDRInterface {
public:
    virtual ~SDRInterface() = default;

    // Device management
    virtual bool initialize(const SDRConfig& config) = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool isRunning() const = 0;

    // Frequency control
    virtual bool setFrequency(Frequency freq) = 0;
    virtual Frequency getFrequency() const = 0;

    // Gain control
    virtual bool setGain(double gain) = 0;
    virtual double getGain() const = 0;
    virtual bool setAutoGain(bool enable) = 0;

    // Sample rate control
    virtual bool setSampleRate(uint32_t rate) = 0;
    virtual uint32_t getSampleRate() const = 0;

    // PPM correction
    virtual bool setPPMCorrection(int32_t ppm) = 0;

    // Callback registration
    virtual void setSampleCallback(SampleCallback callback) = 0;

    // Statistics
    virtual size_t getDroppedSamples() const = 0;
    virtual double getRSSI() const = 0;

    // Device info
    virtual std::string getDeviceInfo() const = 0;
};

} // namespace TrunkSDR

#endif // SDR_INTERFACE_H
