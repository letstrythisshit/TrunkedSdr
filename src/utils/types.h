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
    DMR_TIER2,
    DMR_TIER3,
    NXDN,
    NXDN_NEXEDGE,
    TETRA,
    TETRA_EMERGENCY,
    DPMR,
    DPMR_MODE2,
    TETRAPOL,
    PMR446,
    UNKNOWN
};

// Modulation types
enum class ModulationType {
    FM,
    C4FM,
    FSK,
    FSK4,
    GMSK,
    QPSK,
    DQPSK,
    PI4DQPSK,
    QAM16,
    FFSK
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
    AMBE_PLUS2,
    ACELP,
    ACELP_4567,
    ACELP_7200,
    PROVOICE,
    DMR_CODEC,
    CODEC2,
    VSELP
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

// European-specific encryption types
enum class EncryptionType {
    NONE,
    TEA1,
    TEA2,
    TEA3,
    TEA4,
    ARC4,
    AES128,
    AES256,
    UNKNOWN_ENCRYPTED
};

// DMR-specific data structures
struct DMRColorCode {
    uint8_t code;  // 0-15
};

// TETRA-specific data structures
struct TETRAInfo {
    uint8_t mcc;           // Mobile Country Code
    uint16_t mnc;          // Mobile Network Code
    uint8_t color_code;    // Color code
    EncryptionType encryption;
    bool is_emergency_services;
};

// Call grant information (extended for European protocols)
struct EuropeanCallGrant {
    TalkgroupID talkgroup;
    RadioID radio_id;
    Frequency frequency;
    CallType type;
    Priority priority;
    uint64_t timestamp;
    bool encrypted;
    EncryptionType encryption_type;
    uint8_t color_code;      // For DMR/dPMR/NXDN
    uint8_t slot_number;     // For TDMA systems (DMR, dPMR)
    std::string talker_alias; // DMR talker alias
};

// Constants
constexpr size_t DEFAULT_BUFFER_SIZE = 256 * 1024; // 256K samples
constexpr uint32_t DEFAULT_SAMPLE_RATE = 2048000;  // 2.048 MSPS
constexpr uint32_t AUDIO_SAMPLE_RATE = 8000;       // 8 kHz audio output
constexpr size_t AUDIO_BUFFER_FRAMES = 160;        // 20ms at 8kHz

// European protocol constants
constexpr uint32_t TETRA_SYMBOL_RATE = 18000;     // 18 ksps
constexpr uint32_t DMR_SYMBOL_RATE = 4800;        // 4800 sps
constexpr uint32_t NXDN_SYMBOL_RATE = 2400;       // 2400 sps
constexpr uint32_t DPMR_SYMBOL_RATE = 2400;       // 2400 sps
constexpr float TETRA_CHANNEL_SPACING = 25000.0;  // 25 kHz
constexpr float DMR_CHANNEL_SPACING = 12500.0;    // 12.5 kHz
constexpr float NXDN_CHANNEL_SPACING = 6250.0;    // 6.25 kHz

} // namespace TrunkSDR

#endif // TYPES_H
