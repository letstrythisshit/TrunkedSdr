#ifndef C4FM_DEMOD_H
#define C4FM_DEMOD_H

#include "demodulator.h"
#include "filters.h"
#include <memory>

namespace TrunkSDR {

// C4FM (4-level FSK) demodulator specifically for P25
class C4FMDemodulator : public Demodulator {
public:
    C4FMDemodulator();
    ~C4FMDemodulator() override = default;

    void initialize(uint32_t sample_rate) override;
    void process(const Complex* samples, size_t count) override;
    void reset() override;

private:
    void processSample(const Complex& sample);
    int sliceSymbol(float deviation);

    uint32_t sample_rate_;
    static constexpr uint32_t SYMBOL_RATE = 4800;

    Complex prev_sample_;
    std::unique_ptr<FIRFilter> baseband_filter_;
    std::unique_ptr<FIRFilter> symbol_filter_;

    std::vector<float> symbol_buffer_;
    size_t samples_per_symbol_;
    size_t sample_counter_;
    float symbol_sync_;
};

} // namespace TrunkSDR

#endif // C4FM_DEMOD_H
