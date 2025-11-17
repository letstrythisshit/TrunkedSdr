#include "call_manager.h"
#include "../utils/logger.h"
#include <chrono>

namespace TrunkSDR {

CallManager::CallManager()
    : total_calls_(0) {
}

bool CallManager::initialize(const AudioConfig& config) {
    audio_config_ = config;

    audio_output_ = std::make_unique<AudioOutput>();
    if (!audio_output_->initialize(config.output_device, config.sample_rate)) {
        LOG_ERROR("Failed to initialize audio output");
        return false;
    }

    if (!audio_output_->start()) {
        LOG_ERROR("Failed to start audio output");
        return false;
    }

    LOG_INFO("Call manager initialized");
    return true;
}

void CallManager::handleGrant(const CallGrant& grant) {
    // Check if talkgroup is enabled
    if (!isTalkgroupEnabled(grant.talkgroup)) {
        LOG_DEBUG("Ignoring grant for disabled talkgroup:", grant.talkgroup);
        return;
    }

    std::lock_guard<std::mutex> lock(calls_mutex_);

    // Check if call already active
    auto it = active_calls_.find(grant.talkgroup);
    if (it != active_calls_.end()) {
        // Update existing call
        it->second.last_activity = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        LOG_DEBUG("Updated existing call for TG:", grant.talkgroup);
        return;
    }

    // Create new call
    ActiveCall call;
    call.grant = grant;
    call.start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    call.last_activity = call.start_time;
    call.frame_count = 0;
    call.recording = audio_config_.record_calls;

    active_calls_[grant.talkgroup] = call;
    total_calls_++;

    LOG_INFO("New call started: TG =", grant.talkgroup,
             "Freq =", grant.frequency,
             "Source =", grant.radio_id);
}

void CallManager::handleAudioFrame(TalkgroupID talkgroup, const AudioBuffer& audio) {
    std::lock_guard<std::mutex> lock(calls_mutex_);

    auto it = active_calls_.find(talkgroup);
    if (it == active_calls_.end()) {
        LOG_WARNING("Received audio for inactive call:", talkgroup);
        return;
    }

    // Update activity
    it->second.last_activity = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    it->second.frame_count++;

    // Create audio frame
    AudioFrame frame;
    frame.samples = audio;
    frame.talkgroup = talkgroup;
    frame.radio_id = it->second.grant.radio_id;
    frame.timestamp = it->second.last_activity;
    frame.rssi = -60.0;  // Placeholder

    // Queue for playback
    if (audio_output_) {
        audio_output_->queueAudio(frame);
    }

    // TODO: Record to file if enabled
}

void CallManager::endCall(TalkgroupID talkgroup) {
    std::lock_guard<std::mutex> lock(calls_mutex_);

    auto it = active_calls_.find(talkgroup);
    if (it == active_calls_.end()) {
        return;
    }

    uint64_t duration = it->second.last_activity - it->second.start_time;

    LOG_INFO("Call ended: TG =", talkgroup,
             "Duration =", duration, "ms",
             "Frames =", it->second.frame_count);

    active_calls_.erase(it);
}

bool CallManager::isCallActive(TalkgroupID talkgroup) const {
    std::lock_guard<std::mutex> lock(calls_mutex_);
    return active_calls_.count(talkgroup) > 0;
}

ActiveCall* CallManager::getActiveCall(TalkgroupID talkgroup) {
    std::lock_guard<std::mutex> lock(calls_mutex_);

    auto it = active_calls_.find(talkgroup);
    if (it != active_calls_.end()) {
        return &it->second;
    }
    return nullptr;
}

void CallManager::enableTalkgroup(TalkgroupID talkgroup, Priority priority) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    enabled_talkgroups_[talkgroup] = true;
    talkgroup_priorities_[talkgroup] = priority;
    LOG_INFO("Enabled talkgroup:", talkgroup, "with priority:", static_cast<int>(priority));
}

void CallManager::disableTalkgroup(TalkgroupID talkgroup) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    enabled_talkgroups_[talkgroup] = false;
    LOG_INFO("Disabled talkgroup:", talkgroup);
}

bool CallManager::isTalkgroupEnabled(TalkgroupID talkgroup) const {
    std::lock_guard<std::mutex> lock(config_mutex_);

    auto it = enabled_talkgroups_.find(talkgroup);
    if (it != enabled_talkgroups_.end()) {
        return it->second;
    }

    // If not explicitly configured, allow all
    return enabled_talkgroups_.empty();
}

void CallManager::setTalkgroupPriority(TalkgroupID talkgroup, Priority priority) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    talkgroup_priorities_[talkgroup] = priority;
}

Priority CallManager::getTalkgroupPriority(TalkgroupID talkgroup) const {
    std::lock_guard<std::mutex> lock(config_mutex_);

    auto it = talkgroup_priorities_.find(talkgroup);
    if (it != talkgroup_priorities_.end()) {
        return it->second;
    }
    return 5;  // Default priority
}

size_t CallManager::getActiveCallCount() const {
    std::lock_guard<std::mutex> lock(calls_mutex_);
    return active_calls_.size();
}

void CallManager::cleanupInactiveCalls() {
    std::lock_guard<std::mutex> lock(calls_mutex_);

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    auto it = active_calls_.begin();
    while (it != active_calls_.end()) {
        if (now - it->second.last_activity > CALL_TIMEOUT_MS) {
            LOG_INFO("Timeout: TG =", it->first);
            it = active_calls_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace TrunkSDR
