#ifndef FSK4_DEMOD_H
#define FSK4_DEMOD_H

#include "demodulator.h"
#include "filters.h"
#include <memory>

namespace TrunkSDR {

/**
 * Enhanced 4-level FSK Demodulator
 *
 * Used for DMR, NXDN, and dPMR digital radio systems
 * - Symbol rates: 2400, 4800 sps
 * - Modulation: 4FSK (4-level Frequency Shift Keying)
 * - Each symbol carries 2 bits (dibits)
 *
 * Improvements over basic FSK:
 * - Better frequency discriminator
 * - Symbol timing recovery
 * - Adaptive decision thresholds
 * - Eye diagram monitoring for quality
 */
class FSK4Demodulator : public Demodulator {
public:
    FSK4Demodulator(uint32_t symbol_rate = 4800);
    ~FSK4Demodulator() override = default;

    void initialize(uint32_t sample_rate) override;
    void process(const Complex* samples, size_t count) override;
    void reset() override;

    void setSymbolRate(uint32_t rate) { symbol_rate_ = rate; }
    void setDeviationHz(float deviation) { deviation_hz_ = deviation; }

    // Quality metrics
    float getEyeOpening() const { return eye_opening_; }
    float getFrequencyError() const { return freq_error_; }

private:
    // Frequency discrimination
    float discriminate(const Complex& sample);

    // Symbol decision
    int quantizeSymbol(float value);
    void updateThresholds(float value, int symbol);

    // Symbol timing recovery
    void timingRecovery(float value);

    // Emit dibit
    void emitDibit(int symbol);

    uint32_t sample_rate_;
    uint32_t symbol_rate_;
    float deviation_hz_;

    // Discriminator state
    Complex prev_sample_;
    float phase_accumulator_;

    // Low-pass filter for discriminator output
    std::unique_ptr<FIRFilter> lpf_;

    // Symbol timing recovery
    size_t samples_per_symbol_;
    size_t sample_counter_;
    float timing_error_;
    float mu_;  // Fractional timing offset

    // Adaptive decision thresholds for 4-level detection
    float threshold_low_;   // Between symbols 0 and 1
    float threshold_mid_;   // Between symbols 1 and 2
    float threshold_high_;  // Between symbols 2 and 3

    // Symbol value tracking for adaptive thresholds
    float symbol_0_avg_;
    float symbol_1_avg_;
    float symbol_2_avg_;
    float symbol_3_avg_;

    // Quality metrics
    float eye_opening_;
    float freq_error_;
    std::vector<float> symbol_history_;
};

} // namespace TrunkSDR

#endif // FSK4_DEMOD_H
