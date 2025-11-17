#ifndef RTLSDR_SOURCE_H
#define RTLSDR_SOURCE_H

#include "sdr_interface.h"
#include <rtl-sdr.h>
#include <memory>
#include <vector>

namespace TrunkSDR {

class RTLSDRSource : public SDRInterface {
public:
    RTLSDRSource();
    ~RTLSDRSource() override;

    // SDRInterface implementation
    bool initialize(const SDRConfig& config) override;
    bool start() override;
    bool stop() override;
    bool isRunning() const override { return running_; }

    bool setFrequency(Frequency freq) override;
    Frequency getFrequency() const override { return current_frequency_; }

    bool setGain(double gain) override;
    double getGain() const override;
    bool setAutoGain(bool enable) override;

    bool setSampleRate(uint32_t rate) override;
    uint32_t getSampleRate() const override { return sample_rate_; }

    bool setPPMCorrection(int32_t ppm) override;

    void setSampleCallback(SampleCallback callback) override {
        sample_callback_ = callback;
    }

    size_t getDroppedSamples() const override { return dropped_samples_; }
    double getRSSI() const override;

    std::string getDeviceInfo() const override;

    // Static utility functions
    static uint32_t getDeviceCount();
    static std::string getDeviceName(uint32_t index);

private:
    static void rtlsdrCallback(unsigned char* buf, uint32_t len, void* ctx);
    void processBuffer(unsigned char* buf, uint32_t len);
    void readerThread();

    rtlsdr_dev_t* device_;
    std::atomic<bool> running_;
    std::thread reader_thread_;

    Frequency current_frequency_;
    uint32_t sample_rate_;
    double gain_;
    bool auto_gain_;

    SampleCallback sample_callback_;
    std::vector<Complex> conversion_buffer_;

    std::atomic<size_t> dropped_samples_;
};

} // namespace TrunkSDR

#endif // RTLSDR_SOURCE_H
