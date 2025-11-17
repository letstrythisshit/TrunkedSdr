#ifndef FILTERS_H
#define FILTERS_H

#include "../utils/types.h"
#include <vector>
#include <cmath>

namespace TrunkSDR {

// FIR (Finite Impulse Response) filter
class FIRFilter {
public:
    FIRFilter() = default;

    void setTaps(const std::vector<float>& taps) {
        taps_ = taps;
        buffer_.resize(taps_.size(), 0.0f);
        buffer_index_ = 0;
    }

    float process(float input) {
        buffer_[buffer_index_] = input;

        float output = 0.0f;
        size_t idx = buffer_index_;

        for (size_t i = 0; i < taps_.size(); i++) {
            output += taps_[i] * buffer_[idx];
            idx = (idx == 0) ? taps_.size() - 1 : idx - 1;
        }

        buffer_index_ = (buffer_index_ + 1) % taps_.size();
        return output;
    }

    Complex process(const Complex& input) {
        float real = process(input.real());
        float imag = process(input.imag());
        return Complex(real, imag);
    }

    void reset() {
        std::fill(buffer_.begin(), buffer_.end(), 0.0f);
        buffer_index_ = 0;
    }

    // Create low-pass filter using windowed-sinc method
    static std::vector<float> createLowPassTaps(
        uint32_t sample_rate,
        float cutoff_freq,
        size_t num_taps = 51
    ) {
        std::vector<float> taps(num_taps);
        float fc = cutoff_freq / sample_rate;
        int M = num_taps - 1;

        for (size_t i = 0; i < num_taps; i++) {
            int n = i - M/2;

            // Sinc function
            float h;
            if (n == 0) {
                h = 2.0f * fc;
            } else {
                h = std::sin(2.0f * M_PI * fc * n) / (M_PI * n);
            }

            // Hamming window
            float w = 0.54f - 0.46f * std::cos(2.0f * M_PI * i / M);

            taps[i] = h * w;
        }

        // Normalize
        float sum = 0.0f;
        for (float tap : taps) sum += tap;
        for (float& tap : taps) tap /= sum;

        return taps;
    }

    // Create band-pass filter
    static std::vector<float> createBandPassTaps(
        uint32_t sample_rate,
        float low_freq,
        float high_freq,
        size_t num_taps = 51
    ) {
        auto low_pass = createLowPassTaps(sample_rate, high_freq, num_taps);
        auto high_pass_base = createLowPassTaps(sample_rate, low_freq, num_taps);

        std::vector<float> taps(num_taps);
        for (size_t i = 0; i < num_taps; i++) {
            // Band-pass = low-pass - high-pass
            taps[i] = low_pass[i];
            if (i == num_taps / 2) {
                taps[i] -= (1.0f - high_pass_base[i]);
            } else {
                taps[i] += high_pass_base[i];
            }
        }

        return taps;
    }

private:
    std::vector<float> taps_;
    std::vector<float> buffer_;
    size_t buffer_index_;
};

// IIR (Infinite Impulse Response) filter - Simple 1st order
class IIRFilter {
public:
    IIRFilter(float alpha = 0.1f) : alpha_(alpha), output_(0.0f) {}

    float process(float input) {
        output_ = alpha_ * input + (1.0f - alpha_) * output_;
        return output_;
    }

    void reset() {
        output_ = 0.0f;
    }

    void setAlpha(float alpha) {
        alpha_ = alpha;
    }

private:
    float alpha_;
    float output_;
};

// Automatic Gain Control
class AGC {
public:
    AGC(float attack = 0.1f, float decay = 0.001f, float reference = 0.5f)
        : attack_(attack)
        , decay_(decay)
        , reference_(reference)
        , gain_(1.0f) {}

    float process(float input) {
        float magnitude = std::abs(input);

        // Update gain
        float error = reference_ - magnitude;
        if (error < 0) {
            // Signal too strong, decrease gain quickly (attack)
            gain_ *= (1.0f - attack_);
        } else {
            // Signal too weak, increase gain slowly (decay)
            gain_ *= (1.0f + decay_);
        }

        // Limit gain
        if (gain_ < 0.001f) gain_ = 0.001f;
        if (gain_ > 1000.0f) gain_ = 1000.0f;

        return input * gain_;
    }

    Complex process(const Complex& input) {
        float magnitude = std::abs(input);

        float error = reference_ - magnitude;
        if (error < 0) {
            gain_ *= (1.0f - attack_);
        } else {
            gain_ *= (1.0f + decay_);
        }

        if (gain_ < 0.001f) gain_ = 0.001f;
        if (gain_ > 1000.0f) gain_ = 1000.0f;

        return input * gain_;
    }

    void reset() {
        gain_ = 1.0f;
    }

    float getGain() const { return gain_; }

private:
    float attack_;
    float decay_;
    float reference_;
    float gain_;
};

} // namespace TrunkSDR

#endif // FILTERS_H
