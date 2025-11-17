#include "dqpsk_demod.h"
#include "../utils/logger.h"
#include <algorithm>
#include <cstring>

namespace TrunkSDR {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;

DQPSKDemodulator::DQPSKDemodulator(uint32_t symbol_rate, float rolloff)
    : symbol_rate_(symbol_rate),
      rolloff_(rolloff),
      carrier_bw_(0.01f),
      timing_bw_(0.01f),
      rrc_index_(0),
      carrier_phase_(0.0f),
      carrier_freq_(0.0f),
      carrier_nco_(1.0f, 0.0f),
      timing_error_(0.0f),
      timing_phase_(0.0f),
      timing_freq_(0.0f),
      samples_per_symbol_(0),
      sample_counter_(0),
      symbol_early_(0.0f, 0.0f),
      symbol_prompt_(0.0f, 0.0f),
      symbol_late_(0.0f, 0.0f),
      prev_symbol_(1.0f, 0.0f),
      prev_phase_index_(0),
      phase_offset_(0.0f),
      alternate_constellation_(false),
      evm_(0.0f),
      symbols_demodulated_(0) {
}

void DQPSKDemodulator::initialize(uint32_t sample_rate) {
    sample_rate_ = sample_rate;
    samples_per_symbol_ = sample_rate_ / symbol_rate_;

    // Design root-raised-cosine matched filter
    designRRCFilter();

    // Initialize Costas loop parameters
    float damping = 0.707f;  // Critically damped
    float denom = (1.0f + 2.0f * damping * carrier_bw_ + carrier_bw_ * carrier_bw_);
    carrier_alpha_ = (4.0f * damping * carrier_bw_) / denom;
    carrier_beta_ = (4.0f * carrier_bw_ * carrier_bw_) / denom;

    // Initialize Gardner timing recovery parameters
    denom = (1.0f + 2.0f * damping * timing_bw_ + timing_bw_ * timing_bw_);
    timing_alpha_ = (4.0f * damping * timing_bw_) / denom;
    timing_beta_ = (4.0f * timing_bw_ * timing_bw_) / denom;
    timing_freq_ = 1.0f / static_cast<float>(samples_per_symbol_);

    Logger::info("DQPSK Demodulator initialized: symbol_rate=%u, sample_rate=%u, sps=%zu",
                 symbol_rate_, sample_rate_, samples_per_symbol_);

    reset();
}

void DQPSKDemodulator::reset() {
    carrier_phase_ = 0.0f;
    carrier_freq_ = 0.0f;
    carrier_nco_ = Complex(1.0f, 0.0f);
    timing_error_ = 0.0f;
    timing_phase_ = 0.0f;
    sample_counter_ = 0;
    prev_symbol_ = Complex(1.0f, 0.0f);
    prev_phase_index_ = 0;
    alternate_constellation_ = false;
    symbols_demodulated_ = 0;

    if (!rrc_buffer_.empty()) {
        std::fill(rrc_buffer_.begin(), rrc_buffer_.end(), Complex(0.0f, 0.0f));
    }
}

void DQPSKDemodulator::designRRCFilter() {
    // Design root-raised-cosine filter
    // Filter length: 8 symbols worth of taps
    size_t filter_span = 8;
    size_t num_taps = filter_span * samples_per_symbol_ + 1;
    rrc_taps_.resize(num_taps);
    rrc_buffer_.resize(num_taps, Complex(0.0f, 0.0f));

    float T = 1.0f / static_cast<float>(symbol_rate_);
    float Ts = 1.0f / static_cast<float>(sample_rate_);
    int center = static_cast<int>(num_taps / 2);

    for (size_t i = 0; i < num_taps; i++) {
        float t = static_cast<float>(static_cast<int>(i) - center) * Ts;

        if (t == 0.0f) {
            rrc_taps_[i] = (1.0f / T) * (1.0f + rolloff_ * (4.0f / PI - 1.0f));
        } else if (std::abs(std::abs(t) - T / (4.0f * rolloff_)) < 1e-6f) {
            float tmp = rolloff_ / T;
            rrc_taps_[i] = tmp * ((1.0f + 2.0f / PI) * std::sin(PI / (4.0f * rolloff_)) +
                                  (1.0f - 2.0f / PI) * std::cos(PI / (4.0f * rolloff_)));
        } else {
            float num = std::sin(PI * t / T * (1.0f - rolloff_)) +
                       4.0f * rolloff_ * t / T * std::cos(PI * t / T * (1.0f + rolloff_));
            float denom = PI * t / T * (1.0f - std::pow(4.0f * rolloff_ * t / T, 2.0f));
            rrc_taps_[i] = (1.0f / T) * (num / denom);
        }
    }

    // Normalize filter
    float sum = 0.0f;
    for (float tap : rrc_taps_) {
        sum += tap * tap;
    }
    float norm = std::sqrt(sum);
    for (float& tap : rrc_taps_) {
        tap /= norm;
    }
}

Complex DQPSKDemodulator::rrcFilter(Complex sample) {
    // Shift buffer
    rrc_buffer_[rrc_index_] = sample;
    rrc_index_ = (rrc_index_ + 1) % rrc_buffer_.size();

    // Convolve
    Complex output(0.0f, 0.0f);
    size_t buf_idx = rrc_index_;
    for (size_t i = 0; i < rrc_taps_.size(); i++) {
        buf_idx = (buf_idx > 0) ? buf_idx - 1 : rrc_buffer_.size() - 1;
        output += rrc_buffer_[buf_idx] * rrc_taps_[i];
    }

    return output;
}

Complex DQPSKDemodulator::carrierTrack(Complex sample) {
    // Rotate by NCO to remove carrier offset
    Complex rotated = sample * std::conj(carrier_nco_);

    // Compute phase error using Costas (QPSK)
    float error = phaseError(rotated);

    // Update loop filter
    carrier_freq_ += carrier_beta_ * error;
    carrier_phase_ += carrier_freq_ + carrier_alpha_ * error;

    // Wrap phase
    while (carrier_phase_ > TWO_PI) carrier_phase_ -= TWO_PI;
    while (carrier_phase_ < -TWO_PI) carrier_phase_ += TWO_PI;

    // Update NCO
    carrier_nco_ = Complex(std::cos(carrier_phase_), std::sin(carrier_phase_));

    return rotated;
}

float DQPSKDemodulator::phaseError(Complex sample) {
    // Costas loop error detector for QPSK
    // Error = I*Q' - Q*I' where ' denotes sign
    float I = sample.real();
    float Q = sample.imag();
    float error = 0.0f;

    if (I >= 0.0f && Q >= 0.0f) {
        error = -I + Q;
    } else if (I < 0.0f && Q >= 0.0f) {
        error = -I - Q;
    } else if (I < 0.0f && Q < 0.0f) {
        error = I - Q;
    } else {
        error = I + Q;
    }

    return error;
}

void DQPSKDemodulator::timingRecovery(Complex sample) {
    timing_phase_ += timing_freq_;

    if (timing_phase_ >= 1.0f) {
        timing_phase_ -= 1.0f;

        // Compute Gardner timing error
        float error = gardnerError(symbol_early_, symbol_prompt_, symbol_late_);

        // Update loop filter
        timing_freq_ += timing_beta_ * error;
        timing_freq_ = std::max(0.9f / samples_per_symbol_,
                               std::min(1.1f / samples_per_symbol_, timing_freq_));

        // Emit symbol for decoding
        int sym = demodulateSymbol(symbol_prompt_);
        if (sym >= 0) {
            uint8_t bit0, bit1;
            differentialDecode(sym, bit0, bit1);
            emitSymbol(static_cast<float>(bit0 * 2 + bit1));
            symbols_demodulated_++;
        }

        // Shift symbol buffer
        symbol_early_ = symbol_prompt_;
        symbol_prompt_ = symbol_late_;
    }

    symbol_late_ = sample;
}

float DQPSKDemodulator::gardnerError(Complex early, Complex prompt, Complex late) {
    // Gardner timing error detector
    // error = (late - early) * conj(prompt)
    Complex diff = late - early;
    Complex error_vec = diff * std::conj(prompt);
    return error_vec.real();
}

int DQPSKDemodulator::demodulateSymbol(Complex sample) {
    // Normalize
    float mag = std::abs(sample);
    if (mag < 1e-6f) {
        return -1;  // Invalid symbol
    }

    // Update EVM (Error Vector Magnitude)
    evm_ = 0.9f * evm_ + 0.1f * (1.0f - mag);

    Complex normalized = sample / mag;

    // Compute phase
    float phase = std::arg(normalized);

    // Map to symbol (0, 1, 2, 3)
    int symbol;
    if (phase >= -PI / 4.0f && phase < PI / 4.0f) {
        symbol = 0;
    } else if (phase >= PI / 4.0f && phase < 3.0f * PI / 4.0f) {
        symbol = 1;
    } else if (phase >= -3.0f * PI / 4.0f && phase < -PI / 4.0f) {
        symbol = 3;
    } else {
        symbol = 2;
    }

    return symbol;
}

void DQPSKDemodulator::differentialDecode(int symbol, uint8_t& bit0, uint8_t& bit1) {
    // π/4-DQPSK differential decoding
    // The transmitted information is in the phase difference between consecutive symbols

    // Compute differential phase
    int diff = (symbol - prev_phase_index_ + 4) % 4;

    // Map differential phase to dibits according to π/4-DQPSK
    // The constellation alternates by π/4 rotation
    if (alternate_constellation_) {
        // Odd symbols: π/4, 3π/4, 5π/4, 7π/4
        switch (diff) {
            case 0: bit0 = 0; bit1 = 0; break;  // 0° differential
            case 1: bit0 = 0; bit1 = 1; break;  // 90° differential
            case 2: bit0 = 1; bit1 = 1; break;  // 180° differential
            case 3: bit0 = 1; bit1 = 0; break;  // 270° differential
        }
    } else {
        // Even symbols: 0, π/2, π, 3π/2
        switch (diff) {
            case 0: bit0 = 0; bit1 = 0; break;
            case 1: bit0 = 0; bit1 = 1; break;
            case 2: bit0 = 1; bit1 = 1; break;
            case 3: bit0 = 1; bit1 = 0; break;
        }
    }

    prev_phase_index_ = symbol;
    alternate_constellation_ = !alternate_constellation_;
}

void DQPSKDemodulator::emitSymbol(float symbol) {
    if (symbol_callback_) {
        symbol_callback_(&symbol, 1);
    }
}

void DQPSKDemodulator::process(const Complex* samples, size_t count) {
    for (size_t i = 0; i < count; i++) {
        // 1. Matched filtering (RRC)
        Complex filtered = rrcFilter(samples[i]);

        // 2. Carrier recovery (Costas loop)
        Complex carrier_corrected = carrierTrack(filtered);

        // 3. Symbol timing recovery (Gardner)
        timingRecovery(carrier_corrected);
    }
}

} // namespace TrunkSDR
