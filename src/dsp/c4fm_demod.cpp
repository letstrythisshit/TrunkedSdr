#include "c4fm_demod.h"
#include "../utils/logger.h"
#include <cmath>

namespace TrunkSDR {

C4FMDemodulator::C4FMDemodulator()
    : sample_rate_(0)
    , prev_sample_(1, 0)
    , samples_per_symbol_(0)
    , sample_counter_(0)
    , symbol_sync_(0) {
}

void C4FMDemodulator::initialize(uint32_t sample_rate) {
    sample_rate_ = sample_rate;
    samples_per_symbol_ = sample_rate / SYMBOL_RATE;

    LOG_INFO("C4FM demodulator initialized:",
             "sample_rate =", sample_rate,
             "symbol_rate =", SYMBOL_RATE,
             "samples_per_symbol =", samples_per_symbol_);

    // Baseband filter - remove high frequency noise
    auto baseband_taps = FIRFilter::createLowPassTaps(sample_rate, 6000, 51);
    baseband_filter_ = std::make_unique<FIRFilter>();
    baseband_filter_->setTaps(baseband_taps);

    // Symbol shaping filter
    auto symbol_taps = FIRFilter::createLowPassTaps(sample_rate, SYMBOL_RATE * 0.6f, 31);
    symbol_filter_ = std::make_unique<FIRFilter>();
    symbol_filter_->setTaps(symbol_taps);

    reset();
}

void C4FMDemodulator::reset() {
    prev_sample_ = Complex(1, 0);
    sample_counter_ = 0;
    symbol_sync_ = 0;
    symbol_buffer_.clear();

    if (baseband_filter_) baseband_filter_->reset();
    if (symbol_filter_) symbol_filter_->reset();
}

void C4FMDemodulator::process(const Complex* samples, size_t count) {
    for (size_t i = 0; i < count; i++) {
        processSample(samples[i]);
    }
}

void C4FMDemodulator::processSample(const Complex& sample) {
    // Apply baseband filter
    Complex filtered = baseband_filter_->process(sample);

    // FM discriminator
    Complex product = filtered * std::conj(prev_sample_);
    float deviation = std::arg(product);

    // Symbol filter
    deviation = symbol_filter_->process(deviation);

    // Symbol timing recovery (simple)
    sample_counter_++;
    if (sample_counter_ >= samples_per_symbol_) {
        sample_counter_ = 0;

        // Slice to symbol level
        int symbol = sliceSymbol(deviation);

        // Output
        float symbol_value = static_cast<float>(symbol);
        symbol_buffer_.push_back(symbol_value);

        if (symbol_buffer_.size() >= 100) {
            if (symbol_callback_) {
                symbol_callback_(symbol_buffer_.data(), symbol_buffer_.size());
            }
            symbol_buffer_.clear();
        }
    }

    prev_sample_ = filtered;
}

int C4FMDemodulator::sliceSymbol(float deviation) {
    // P25 C4FM uses 4 levels: +3, +1, -1, -3
    // Map to symbols 0, 1, 2, 3

    const float THRESHOLD = 0.15f;

    if (deviation > THRESHOLD) {
        return (deviation > 3.0f * THRESHOLD) ? 3 : 2;
    } else {
        return (deviation > -THRESHOLD) ? 1 : 0;
    }
}

} // namespace TrunkSDR
