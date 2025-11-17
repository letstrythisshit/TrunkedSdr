#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <complex>
#include <vector>
#include <string>
#include <memory>

namespace TrunkSDR {

// Complex sample type
using Complex = std::complex<float>;
using ComplexVector = std::vector<Complex>;

// Audio sample types
using AudioSample = int16_t;
using AudioBuffer = std::vector<AudioSample>;

// Frequency in Hz
using Frequency = double;

// Talkgroup and Radio IDs
using TalkgroupID = uint32_t;
using RadioID = uint32_t;
using SystemID = uint32_t;

// Call priority (higher = more important)
using Priority = uint8_t;

// Trunking system types
enum class SystemType {
    P25_PHASE1,
    P25_PHASE2,
    SMARTNET,
    SMARTZONE,
    EDACS,
    LTR,
    DMR,
    NXDN,
    UNKNOWN
};

// Modulation types
enum class ModulationType {
    FM,
    C4FM,
    FSK,
    GMSK,
    QPSK,
    QAM16
};

// Call types
enum class CallType {
    GROUP,
    PRIVATE,
    EMERGENCY,
    ENCRYPTED,
    UNKNOWN
};

// Audio codec types
enum class CodecType {
    ANALOG_FM,
    IMBE,
    AMBE,
    PROVOICE,
    DMR_CODEC,
    CODEC2
};

// System information structure
struct SystemInfo {
    SystemType type;
    SystemID system_id;
    uint16_t nac;  // P25 NAC (Network Access Code)
    uint16_t wacn; // P25 WACN (Wide Area Communications Network)
    std::vector<Frequency> control_channels;
    std::string name;
};

// Call grant information
struct CallGrant {
    TalkgroupID talkgroup;
    RadioID radio_id;
    Frequency frequency;
    CallType type;
    Priority priority;
    uint64_t timestamp;
    bool encrypted;
};

// Audio frame structure
struct AudioFrame {
    AudioBuffer samples;
    TalkgroupID talkgroup;
    RadioID radio_id;
    uint64_t timestamp;
    double rssi; // Received Signal Strength Indicator
};

// SDR configuration
struct SDRConfig {
    uint32_t device_index;
    uint32_t sample_rate;
    double gain;
    int32_t ppm_correction;
    bool auto_gain;
};

// Constants
constexpr size_t DEFAULT_BUFFER_SIZE = 256 * 1024; // 256K samples
constexpr uint32_t DEFAULT_SAMPLE_RATE = 2048000;  // 2.048 MSPS
constexpr uint32_t AUDIO_SAMPLE_RATE = 8000;       // 8 kHz audio output
constexpr size_t AUDIO_BUFFER_FRAMES = 160;        // 20ms at 8kHz

} // namespace TrunkSDR

#endif // TYPES_H
