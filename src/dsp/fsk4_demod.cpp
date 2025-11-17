#include "fsk4_demod.h"
#include "../utils/logger.h"
#include <algorithm>
#include <cmath>

namespace TrunkSDR {

constexpr float PI = 3.14159265358979323846f;

FSK4Demodulator::FSK4Demodulator(uint32_t symbol_rate)
    : symbol_rate_(symbol_rate),
      deviation_hz_(1944.0f),  // DMR deviation
      prev_sample_(0.0f, 0.0f),
      phase_accumulator_(0.0f),
      samples_per_symbol_(0),
      sample_counter_(0),
      timing_error_(0.0f),
      mu_(0.0f),
      threshold_low_(-0.5f),
      threshold_mid_(0.0f),
      threshold_high_(0.5f),
      symbol_0_avg_(-1.0f),
      symbol_1_avg_(-0.33f),
      symbol_2_avg_(0.33f),
      symbol_3_avg_(1.0f),
      eye_opening_(1.0f),
      freq_error_(0.0f) {
}

void FSK4Demodulator::initialize(uint32_t sample_rate) {
    sample_rate_ = sample_rate;
    samples_per_symbol_ = sample_rate_ / symbol_rate_;

    // Design low-pass filter for discriminator output
    // Cutoff at symbol rate to remove high-frequency noise
    float cutoff = static_cast<float>(symbol_rate_) * 1.2f;
    size_t num_taps = 41;

    std::vector<float> taps(num_taps);
    float omega_c = 2.0f * PI * cutoff / static_cast<float>(sample_rate_);

    for (size_t i = 0; i < num_taps; i++) {
        int n = static_cast<int>(i) - static_cast<int>(num_taps / 2);
        if (n == 0) {
            taps[i] = omega_c / PI;
        } else {
            taps[i] = std::sin(omega_c * n) / (PI * n);
        }

        // Hamming window
        taps[i] *= 0.54f - 0.46f * std::cos(2.0f * PI * i / (num_taps - 1));
    }

    // Normalize
    float sum = 0.0f;
    for (float tap : taps) {
        sum += tap;
    }
    for (float& tap : taps) {
        tap /= sum;
    }

    lpf_ = std::make_unique<FIRFilter>(taps);

    symbol_history_.reserve(100);

    Logger::info("FSK4 Demodulator initialized: symbol_rate=%u, sample_rate=%u, sps=%zu",
                 symbol_rate_, sample_rate_, samples_per_symbol_);

    reset();
}

void FSK4Demodulator::reset() {
    prev_sample_ = Complex(0.0f, 0.0f);
    phase_accumulator_ = 0.0f;
    sample_counter_ = 0;
    timing_error_ = 0.0f;
    mu_ = 0.0f;

    if (lpf_) {
        lpf_->reset();
    }

    symbol_history_.clear();
}

float FSK4Demodulator::discriminate(const Complex& sample) {
    // FM discriminator: instantaneous frequency is derivative of phase
    // freq = (1/2π) * d(phase)/dt

    // Compute conjugate product for phase difference
    Complex diff = sample * std::conj(prev_sample_);
    prev_sample_ = sample;

    // Extract phase difference
    float phase_diff = std::arg(diff);

    // Convert to frequency deviation
    float freq = phase_diff * static_cast<float>(sample_rate_) / (2.0f * PI);

    return freq;
}

int FSK4Demodulator::quantizeSymbol(float value) {
    // Map to 4 symbols based on adaptive thresholds
    // DMR: -3, -1, +1, +3 (dibits: 00, 01, 10, 11)

    int symbol;
    if (value < threshold_low_) {
        symbol = 0;  // Most negative deviation (-1944 Hz for DMR)
    } else if (value < threshold_mid_) {
        symbol = 1;  // Moderate negative deviation (-648 Hz for DMR)
    } else if (value < threshold_high_) {
        symbol = 2;  // Moderate positive deviation (+648 Hz for DMR)
    } else {
        symbol = 3;  // Most positive deviation (+1944 Hz for DMR)
    }

    // Update adaptive thresholds
    updateThresholds(value, symbol);

    return symbol;
}

void FSK4Demodulator::updateThresholds(float value, int symbol) {
    // Exponentially weighted moving average for symbol centers
    const float alpha = 0.01f;  // Slow adaptation

    switch (symbol) {
        case 0:
            symbol_0_avg_ = (1.0f - alpha) * symbol_0_avg_ + alpha * value;
            break;
        case 1:
            symbol_1_avg_ = (1.0f - alpha) * symbol_1_avg_ + alpha * value;
            break;
        case 2:
            symbol_2_avg_ = (1.0f - alpha) * symbol_2_avg_ + alpha * value;
            break;
        case 3:
            symbol_3_avg_ = (1.0f - alpha) * symbol_3_avg_ + alpha * value;
            break;
    }

    // Update thresholds as midpoints between symbol averages
    threshold_low_ = (symbol_0_avg_ + symbol_1_avg_) / 2.0f;
    threshold_mid_ = (symbol_1_avg_ + symbol_2_avg_) / 2.0f;
    threshold_high_ = (symbol_2_avg_ + symbol_3_avg_) / 2.0f;

    // Calculate eye opening (distance between symbols)
    eye_opening_ = (symbol_3_avg_ - symbol_0_avg_) / 3.0f;
}

void FSK4Demodulator::timingRecovery(float value) {
    // Simple Mueller and Muller timing recovery
    sample_counter_++;

    if (sample_counter_ >= samples_per_symbol_) {
        sample_counter_ = 0;

        // Interpolate symbol at optimal sampling point
        // For now, use nearest sample (simplified)
        int symbol = quantizeSymbol(value);

        emitDibit(symbol);

        // Store for timing error calculation
        symbol_history_.push_back(value);
        if (symbol_history_.size() > 3) {
            symbol_history_.erase(symbol_history_.begin());
        }

        // Calculate timing error (simplified)
        if (symbol_history_.size() >= 3) {
            float error = (symbol_history_[2] - symbol_history_[0]) * symbol_history_[1];
            timing_error_ = 0.9f * timing_error_ + 0.1f * error;

            // Adjust sample counter based on timing error
            mu_ += timing_error_ * 0.01f;
            if (mu_ > 1.0f) {
                mu_ -= 1.0f;
                sample_counter_++;
            } else if (mu_ < -1.0f) {
                mu_ += 1.0f;
                if (sample_counter_ > 0) {
                    sample_counter_--;
                }
            }
        }
    }
}

void FSK4Demodulator::emitDibit(int symbol) {
    // Convert symbol to float for callback
    // 0 -> 0.0, 1 -> 1.0, 2 -> 2.0, 3 -> 3.0
    float sym_float = static_cast<float>(symbol);

    if (symbol_callback_) {
        symbol_callback_(&sym_float, 1);
    }
}

void FSK4Demodulator::process(const Complex* samples, size_t count) {
    for (size_t i = 0; i < count; i++) {
        // 1. Frequency discrimination
        float freq = discriminate(samples[i]);

        // 2. Low-pass filter
        float filtered = lpf_->filter(freq);

        // 3. Symbol timing recovery and decision
        timingRecovery(filtered);
    }
}

} // namespace TrunkSDR
