#ifndef DEMODULATOR_H
#define DEMODULATOR_H

#include "../utils/types.h"
#include <vector>
#include <functional>

namespace TrunkSDR {

// Symbol callback for digital demodulators
using SymbolCallback = std::function<void(const float*, size_t)>;

class Demodulator {
public:
    virtual ~Demodulator() = default;

    virtual void initialize(uint32_t sample_rate) = 0;
    virtual void process(const Complex* samples, size_t count) = 0;
    virtual void reset() = 0;

    void setSymbolCallback(SymbolCallback callback) {
        symbol_callback_ = callback;
    }

protected:
    SymbolCallback symbol_callback_;
};

} // namespace TrunkSDR

#endif // DEMODULATOR_H
