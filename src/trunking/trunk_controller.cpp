#include "trunk_controller.h"
#include "../sdr/rtlsdr_source.h"
#include "../dsp/c4fm_demod.h"
#include "../dsp/fsk_demod.h"
#include "../decoders/p25_decoder.h"
#include "../decoders/smartnet_decoder.h"
#include "../utils/logger.h"

namespace TrunkSDR {

TrunkController::TrunkController()
    : running_(false)
    , current_control_freq_(0)
    , current_voice_freq_(0)
    , voice_active_(false) {
}

TrunkController::~TrunkController() {
    stop();
}

bool TrunkController::initialize(const Config& config) {
    config_ = config;

    LOG_INFO("Initializing trunk controller");
    LOG_INFO("System type:", ConfigParser::systemTypeToString(config.system.type));

    // Initialize SDR for control channel
    control_sdr_ = std::make_unique<RTLSDRSource>();
    if (!control_sdr_->initialize(config.sdr)) {
        LOG_ERROR("Failed to initialize control SDR");
        return false;
    }

    // Initialize demodulator based on system type
    if (config.system.type == SystemType::P25_PHASE1 ||
        config.system.type == SystemType::P25_PHASE2) {
        // C4FM for P25
        control_demod_ = std::make_unique<C4FMDemodulator>();
    } else if (config.system.type == SystemType::SMARTNET ||
               config.system.type == SystemType::SMARTZONE) {
        // FSK2 for SmartNet
        control_demod_ = std::make_unique<FSKDemodulator>(3600, 2);
    } else {
        LOG_ERROR("Unsupported system type");
        return false;
    }

    control_demod_->initialize(config.sdr.sample_rate);

    // Initialize protocol decoder
    if (config.system.type == SystemType::P25_PHASE1 ||
        config.system.type == SystemType::P25_PHASE2) {
        auto p25_decoder = std::make_unique<P25Decoder>();
        p25_decoder->setNAC(config.system.nac);
        protocol_decoder_ = std::move(p25_decoder);
    } else if (config.system.type == SystemType::SMARTNET ||
               config.system.type == SystemType::SMARTZONE) {
        protocol_decoder_ = std::make_unique<SmartNetDecoder>();
    }

    protocol_decoder_->initialize();

    // Set up decoder callback
    protocol_decoder_->setGrantCallback(
        [this](const CallGrant& grant) {
            handleCallGrant(grant);
        }
    );

    // Initialize call manager
    call_manager_ = std::make_unique<CallManager>();
    if (!call_manager_->initialize(config.audio)) {
        LOG_ERROR("Failed to initialize call manager");
        return false;
    }

    // Configure enabled talkgroups
    for (TalkgroupID tg : config.talkgroups.enabled) {
        Priority priority = 5;  // Default
        if (config.talkgroups.priorities.count(tg)) {
            priority = config.talkgroups.priorities.at(tg);
        }
        call_manager_->enableTalkgroup(tg, priority);
    }

    LOG_INFO("Trunk controller initialized successfully");
    return true;
}

bool TrunkController::start() {
    if (running_) {
        LOG_WARNING("Trunk controller already running");
        return true;
    }

    // Tune to first control channel
    if (config_.system.control_channels.empty()) {
        LOG_ERROR("No control channels configured");
        return false;
    }

    if (!tuneToControlChannel(config_.system.control_channels[0])) {
        LOG_ERROR("Failed to tune to control channel");
        return false;
    }

    // Start SDR
    if (!control_sdr_->start()) {
        LOG_ERROR("Failed to start control SDR");
        return false;
    }

    // Set up SDR sample callback
    control_sdr_->setSampleCallback(
        [this](const Complex* samples, size_t count) {
            // Process samples through demodulator
            control_demod_->process(samples, count);
        }
    );

    // Set up demodulator symbol callback
    control_demod_->setSymbolCallback(
        [this](const float* symbols, size_t count) {
            // Process symbols through protocol decoder
            protocol_decoder_->processSymbols(symbols, count);
        }
    );

    running_ = true;

    LOG_INFO("Trunk controller started");
    return true;
}

bool TrunkController::stop() {
    if (!running_) {
        return true;
    }

    running_ = false;

    if (control_sdr_) {
        control_sdr_->stop();
    }

    if (voice_sdr_) {
        voice_sdr_->stop();
    }

    LOG_INFO("Trunk controller stopped");
    return true;
}

bool TrunkController::tuneToControlChannel(Frequency freq) {
    if (!control_sdr_) {
        LOG_ERROR("Control SDR not initialized");
        return false;
    }

    if (!control_sdr_->setFrequency(freq)) {
        LOG_ERROR("Failed to tune to control frequency:", freq);
        return false;
    }

    current_control_freq_ = freq;
    LOG_INFO("Tuned to control channel:", freq, "Hz");
    return true;
}

bool TrunkController::tuneToVoiceChannel(Frequency freq) {
    // For single-SDR operation, retune the control SDR
    // In multi-SDR mode, use a separate voice SDR

    if (!control_sdr_) {
        return false;
    }

    // TODO: Implement voice channel following
    // This would require:
    // 1. Saving control channel state
    // 2. Tuning to voice frequency
    // 3. Decoding voice
    // 4. Returning to control channel

    current_voice_freq_ = freq;
    voice_active_ = true;

    LOG_INFO("Tuned to voice channel:", freq, "Hz");
    return true;
}

void TrunkController::handleCallGrant(const CallGrant& grant) {
    LOG_INFO("Call grant received: TG =", grant.talkgroup,
             "Freq =", grant.frequency);

    // Forward to call manager
    if (call_manager_) {
        call_manager_->handleGrant(grant);
    }

    // TODO: Implement voice channel following
    // For now, we just log the grant
}

} // namespace TrunkSDR
