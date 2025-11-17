#ifndef FSK_DEMOD_H
#define FSK_DEMOD_H

#include "demodulator.h"
#include "filters.h"
#include <memory>

namespace TrunkSDR {

// FSK4 demodulator for P25 and other systems
class FSKDemodulator : public Demodulator {
public:
    FSKDemodulator(uint32_t symbol_rate = 4800, uint32_t levels = 4);
    ~FSKDemodulator() override = default;

    void initialize(uint32_t sample_rate) override;
    void process(const Complex* samples, size_t count) override;
    void reset() override;

    void setSymbolRate(uint32_t rate) { symbol_rate_ = rate; }
    void setLevels(uint32_t levels) { levels_ = levels; }

private:
    float discriminate(const Complex& sample);
    int quantizeSymbol(float value);

    uint32_t sample_rate_;
    uint32_t symbol_rate_;
    uint32_t levels_;  // 2 for FSK2, 4 for FSK4

    Complex prev_sample_;
    float phase_accumulator_;

    std::unique_ptr<FIRFilter> lpf_;
    std::vector<float> symbol_buffer_;

    size_t samples_per_symbol_;
    size_t sample_counter_;
};

} // namespace TrunkSDR

#endif // FSK_DEMOD_H
