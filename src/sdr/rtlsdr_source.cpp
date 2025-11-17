#include "rtlsdr_source.h"
#include "../utils/logger.h"
#include <cstring>
#include <cmath>

namespace TrunkSDR {

RTLSDRSource::RTLSDRSource()
    : device_(nullptr)
    , running_(false)
    , current_frequency_(0)
    , sample_rate_(DEFAULT_SAMPLE_RATE)
    , gain_(0)
    , auto_gain_(false)
    , dropped_samples_(0) {
}

RTLSDRSource::~RTLSDRSource() {
    stop();
    if (device_) {
        rtlsdr_close(device_);
        device_ = nullptr;
    }
}

bool RTLSDRSource::initialize(const SDRConfig& config) {
    int device_count = rtlsdr_get_device_count();
    if (device_count == 0) {
        LOG_ERROR("No RTL-SDR devices found");
        return false;
    }

    LOG_INFO("Found", device_count, "RTL-SDR device(s)");

    if (config.device_index >= static_cast<uint32_t>(device_count)) {
        LOG_ERROR("Invalid device index:", config.device_index);
        return false;
    }

    int result = rtlsdr_open(&device_, config.device_index);
    if (result < 0) {
        LOG_ERROR("Failed to open RTL-SDR device:", result);
        return false;
    }

    LOG_INFO("Opened RTL-SDR device:", getDeviceInfo());

    // Set sample rate
    if (!setSampleRate(config.sample_rate)) {
        return false;
    }

    // Set PPM correction
    if (!setPPMCorrection(config.ppm_correction)) {
        return false;
    }

    // Set gain
    if (config.auto_gain) {
        setAutoGain(true);
    } else {
        setGain(config.gain);
    }

    // Reset buffer
    rtlsdr_reset_buffer(device_);

    LOG_INFO("RTL-SDR initialized successfully");
    return true;
}

bool RTLSDRSource::start() {
    if (running_) {
        LOG_WARNING("RTL-SDR already running");
        return true;
    }

    if (!device_) {
        LOG_ERROR("Device not initialized");
        return false;
    }

    running_ = true;
    reader_thread_ = std::thread(&RTLSDRSource::readerThread, this);

    LOG_INFO("RTL-SDR started");
    return true;
}

bool RTLSDRSource::stop() {
    if (!running_) {
        return true;
    }

    running_ = false;

    if (device_) {
        rtlsdr_cancel_async(device_);
    }

    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }

    LOG_INFO("RTL-SDR stopped");
    return true;
}

bool RTLSDRSource::setFrequency(Frequency freq) {
    if (!device_) {
        LOG_ERROR("Device not initialized");
        return false;
    }

    int result = rtlsdr_set_center_freq(device_, static_cast<uint32_t>(freq));
    if (result < 0) {
        LOG_ERROR("Failed to set frequency:", freq);
        return false;
    }

    current_frequency_ = freq;
    LOG_DEBUG("Set frequency to", freq, "Hz");
    return true;
}

bool RTLSDRSource::setGain(double gain) {
    if (!device_) {
        LOG_ERROR("Device not initialized");
        return false;
    }

    // RTL-SDR gain is in tenths of dB
    int gain_tenths = static_cast<int>(gain * 10);

    int result = rtlsdr_set_tuner_gain(device_, gain_tenths);
    if (result < 0) {
        LOG_ERROR("Failed to set gain:", gain);
        return false;
    }

    gain_ = gain;
    LOG_DEBUG("Set gain to", gain, "dB");
    return true;
}

double RTLSDRSource::getGain() const {
    if (!device_) {
        return 0;
    }

    int gain_tenths = rtlsdr_get_tuner_gain(device_);
    return gain_tenths / 10.0;
}

bool RTLSDRSource::setAutoGain(bool enable) {
    if (!device_) {
        LOG_ERROR("Device not initialized");
        return false;
    }

    int result = rtlsdr_set_tuner_gain_mode(device_, enable ? 0 : 1);
    if (result < 0) {
        LOG_ERROR("Failed to set auto gain mode");
        return false;
    }

    auto_gain_ = enable;
    LOG_DEBUG("Auto gain:", enable ? "enabled" : "disabled");
    return true;
}

bool RTLSDRSource::setSampleRate(uint32_t rate) {
    if (!device_) {
        LOG_ERROR("Device not initialized");
        return false;
    }

    int result = rtlsdr_set_sample_rate(device_, rate);
    if (result < 0) {
        LOG_ERROR("Failed to set sample rate:", rate);
        return false;
    }

    sample_rate_ = rate;
    LOG_DEBUG("Set sample rate to", rate, "Hz");
    return true;
}

bool RTLSDRSource::setPPMCorrection(int32_t ppm) {
    if (!device_) {
        LOG_ERROR("Device not initialized");
        return false;
    }

    int result = rtlsdr_set_freq_correction(device_, ppm);
    if (result < 0) {
        LOG_ERROR("Failed to set PPM correction:", ppm);
        return false;
    }

    LOG_DEBUG("Set PPM correction to", ppm);
    return true;
}

double RTLSDRSource::getRSSI() const {
    // Simple RSSI calculation based on signal power
    // This is a simplified implementation
    return -50.0; // Placeholder
}

std::string RTLSDRSource::getDeviceInfo() const {
    if (!device_) {
        return "No device";
    }

    char manufacturer[256], product[256], serial[256];
    rtlsdr_get_usb_strings(device_, manufacturer, product, serial);

    return std::string(manufacturer) + " " + product + " (SN: " + serial + ")";
}

uint32_t RTLSDRSource::getDeviceCount() {
    return rtlsdr_get_device_count();
}

std::string RTLSDRSource::getDeviceName(uint32_t index) {
    const char* name = rtlsdr_get_device_name(index);
    return name ? std::string(name) : "";
}

void RTLSDRSource::readerThread() {
    LOG_INFO("Reader thread started");

    const int buf_num = 15;
    const int buf_len = 16384;

    int result = rtlsdr_read_async(device_, rtlsdrCallback, this, buf_num, buf_len);

    if (result < 0) {
        LOG_ERROR("Async read failed:", result);
    }

    LOG_INFO("Reader thread stopped");
}

void RTLSDRSource::rtlsdrCallback(unsigned char* buf, uint32_t len, void* ctx) {
    RTLSDRSource* source = static_cast<RTLSDRSource*>(ctx);
    source->processBuffer(buf, len);
}

void RTLSDRSource::processBuffer(unsigned char* buf, uint32_t len) {
    if (!sample_callback_) {
        return;
    }

    // Convert uint8 I/Q to complex float
    // RTL-SDR provides unsigned 8-bit I/Q pairs [I0, Q0, I1, Q1, ...]
    // We need to convert to [-1.0, 1.0] range

    size_t num_samples = len / 2;
    conversion_buffer_.resize(num_samples);

    for (size_t i = 0; i < num_samples; i++) {
        // Convert [0, 255] to [-1.0, 1.0]
        float I = (buf[2*i] - 127.4f) / 128.0f;
        float Q = (buf[2*i + 1] - 127.4f) / 128.0f;
        conversion_buffer_[i] = Complex(I, Q);
    }

    sample_callback_(conversion_buffer_.data(), num_samples);
}

} // namespace TrunkSDR
