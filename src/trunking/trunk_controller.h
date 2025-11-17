#ifndef TRUNK_CONTROLLER_H
#define TRUNK_CONTROLLER_H

#include "../utils/types.h"
#include "../sdr/sdr_interface.h"
#include "../dsp/demodulator.h"
#include "../decoders/base_decoder.h"
#include "../audio/call_manager.h"
#include <memory>
#include <atomic>
#include <thread>

namespace TrunkSDR {

class TrunkController {
public:
    TrunkController();
    ~TrunkController();

    bool initialize(const Config& config);
    bool start();
    bool stop();

    bool isRunning() const { return running_; }

    // Tuning control
    bool tuneToControlChannel(Frequency freq);
    bool tuneToVoiceChannel(Frequency freq);

    // Get components
    CallManager* getCallManager() { return call_manager_.get(); }

private:
    void controlChannelThread();
    void voiceChannelThread();

    void handleCallGrant(const CallGrant& grant);

    Config config_;

    // SDR resources
    std::unique_ptr<SDRInterface> control_sdr_;
    std::unique_ptr<SDRInterface> voice_sdr_;  // Optional second SDR

    // DSP chain
    std::unique_ptr<Demodulator> control_demod_;
    std::unique_ptr<Demodulator> voice_demod_;

    // Protocol decoder
    std::unique_ptr<BaseDecoder> protocol_decoder_;

    // Call management
    std::unique_ptr<CallManager> call_manager_;

    // Threading
    std::atomic<bool> running_;
    std::thread control_thread_;
    std::thread voice_thread_;

    // Current state
    Frequency current_control_freq_;
    Frequency current_voice_freq_;
    std::atomic<bool> voice_active_;
};

} // namespace TrunkSDR

#endif // TRUNK_CONTROLLER_H
