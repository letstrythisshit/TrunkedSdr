#include "fsk_demod.h"
#include "../utils/logger.h"
#include <cmath>

namespace TrunkSDR {

FSKDemodulator::FSKDemodulator(uint32_t symbol_rate, uint32_t levels)
    : sample_rate_(0)
    , symbol_rate_(symbol_rate)
    , levels_(levels)
    , prev_sample_(0, 0)
    , phase_accumulator_(0)
    , samples_per_symbol_(0)
    , sample_counter_(0) {
}

void FSKDemodulator::initialize(uint32_t sample_rate) {
    sample_rate_ = sample_rate;
    samples_per_symbol_ = sample_rate / symbol_rate_;

    LOG_INFO("FSK demodulator initialized:",
             "sample_rate =", sample_rate,
             "symbol_rate =", symbol_rate_,
             "levels =", levels_,
             "samples_per_symbol =", samples_per_symbol_);

    // Create low-pass filter for baseband
    float cutoff = symbol_rate_ * 1.2f;  // Slightly wider than symbol rate
    auto taps = FIRFilter::createLowPassTaps(sample_rate, cutoff, 51);
    lpf_ = std::make_unique<FIRFilter>();
    lpf_->setTaps(taps);

    reset();
}

void FSKDemodulator::reset() {
    prev_sample_ = Complex(1, 0);
    phase_accumulator_ = 0;
    sample_counter_ = 0;
    symbol_buffer_.clear();

    if (lpf_) {
        lpf_->reset();
    }
}

void FSKDemodulator::process(const Complex* samples, size_t count) {
    for (size_t i = 0; i < count; i++) {
        // FM discriminator
        float deviation = discriminate(samples[i]);

        // Low-pass filter
        deviation = lpf_->process(deviation);

        // Symbol timing
        sample_counter_++;
        if (sample_counter_ >= samples_per_symbol_) {
            sample_counter_ = 0;

            // Quantize to symbol level
            int symbol = quantizeSymbol(deviation);

            // Output symbol
            float symbol_value = static_cast<float>(symbol);
            symbol_buffer_.push_back(symbol_value);

            // Call callback if we have enough symbols
            if (symbol_buffer_.size() >= 100) {
                if (symbol_callback_) {
                    symbol_callback_(symbol_buffer_.data(), symbol_buffer_.size());
                }
                symbol_buffer_.clear();
            }
        }

        prev_sample_ = samples[i];
    }
}

float FSKDemodulator::discriminate(const Complex& sample) {
    // FM discriminator: atan2(Q, I)
    // Frequency deviation = d(phase)/dt

    Complex product = sample * std::conj(prev_sample_);
    float instantaneous_phase = std::arg(product);

    return instantaneous_phase;
}

int FSKDemodulator::quantizeSymbol(float value) {
    // Map deviation to symbol levels
    // For FSK4: -3, -1, +1, +3 -> 0, 1, 2, 3

    if (levels_ == 4) {
        // C4FM levels
        if (value < -0.15f) return 0;
        else if (value < 0.0f) return 1;
        else if (value < 0.15f) return 2;
        else return 3;
    } else if (levels_ == 2) {
        // Binary FSK
        return (value > 0.0f) ? 1 : 0;
    }

    return 0;
}

} // namespace TrunkSDR
