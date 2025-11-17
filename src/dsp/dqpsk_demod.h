#ifndef DQPSK_DEMOD_H
#define DQPSK_DEMOD_H

#include "demodulator.h"
#include "filters.h"
#include <memory>
#include <cmath>

namespace TrunkSDR {

/**
 * π/4-DQPSK (Differential Quadrature Phase Shift Keying) Demodulator
 *
 * Used for TETRA (Terrestrial Trunked Radio) systems
 * - Symbol rate: 18,000 symbols/second
 * - Modulation: π/4-DQPSK with differential encoding
 * - Roll-off factor: 0.35 (root-raised-cosine)
 *
 * This is significantly more complex than FSK demodulation and requires:
 * - Matched filtering (RRC)
 * - Carrier recovery (Costas loop)
 * - Symbol timing recovery (Gardner detector)
 * - Differential demodulation
 * - Phase ambiguity resolution
 */
class DQPSKDemodulator : public Demodulator {
public:
    DQPSKDemodulator(uint32_t symbol_rate = 18000, float rolloff = 0.35);
    ~DQPSKDemodulator() override = default;

    void initialize(uint32_t sample_rate) override;
    void process(const Complex* samples, size_t count) override;
    void reset() override;

    void setSymbolRate(uint32_t rate) { symbol_rate_ = rate; }
    void setRolloffFactor(float rolloff) { rolloff_ = rolloff; }
    void setCarrierTrackingBandwidth(float bandwidth) { carrier_bw_ = bandwidth; }
    void setTimingTrackingBandwidth(float bandwidth) { timing_bw_ = bandwidth; }

private:
    // Root-raised-cosine matched filter
    void designRRCFilter();
    Complex rrcFilter(Complex sample);

    // Carrier recovery (Costas loop for QPSK)
    Complex carrierTrack(Complex sample);
    float phaseError(Complex sample);

    // Symbol timing recovery (Gardner detector)
    void timingRecovery(Complex sample);
    float gardnerError(Complex early, Complex prompt, Complex late);

    // Differential demodulation
    int demodulateSymbol(Complex sample);
    void differentialDecode(int symbol, uint8_t& bit0, uint8_t& bit1);

    // Phase constellation mapping for π/4-DQPSK
    void mapPhaseToSymbol(float phase, uint8_t& bit0, uint8_t& bit1);

    // Emit symbol to callback
    void emitSymbol(float symbol);

    uint32_t sample_rate_;
    uint32_t symbol_rate_;
    float rolloff_;
    float carrier_bw_;
    float timing_bw_;

    // RRC filter state
    std::vector<float> rrc_taps_;
    std::vector<Complex> rrc_buffer_;
    size_t rrc_index_;

    // Carrier tracking (Costas loop)
    float carrier_phase_;
    float carrier_freq_;
    Complex carrier_nco_;
    float carrier_alpha_;  // Loop filter coefficient
    float carrier_beta_;   // Loop filter coefficient

    // Timing recovery (Gardner)
    float timing_error_;
    float timing_phase_;
    float timing_freq_;
    float timing_alpha_;
    float timing_beta_;
    size_t samples_per_symbol_;
    size_t sample_counter_;

    // Symbol buffering for Gardner detector
    Complex symbol_early_;
    Complex symbol_prompt_;
    Complex symbol_late_;

    // Differential decoder state
    Complex prev_symbol_;
    int prev_phase_index_;

    // Phase rotation for π/4-DQPSK
    // Constellation rotates by π/4 between symbols
    float phase_offset_;
    bool alternate_constellation_;

    // Statistics
    float evm_;  // Error Vector Magnitude
    size_t symbols_demodulated_;
};

} // namespace TrunkSDR

#endif // DQPSK_DEMOD_H
