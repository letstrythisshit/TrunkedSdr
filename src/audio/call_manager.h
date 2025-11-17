#ifndef CALL_MANAGER_H
#define CALL_MANAGER_H

#include "../utils/types.h"
#include "audio_output.h"
#include <map>
#include <memory>
#include <mutex>

namespace TrunkSDR {

struct ActiveCall {
    CallGrant grant;
    uint64_t start_time;
    uint64_t last_activity;
    size_t frame_count;
    bool recording;
};

class CallManager {
public:
    CallManager();
    ~CallManager() = default;

    bool initialize(const AudioConfig& config);

    // Call lifecycle
    void handleGrant(const CallGrant& grant);
    void handleAudioFrame(TalkgroupID talkgroup, const AudioBuffer& audio);
    void endCall(TalkgroupID talkgroup);

    // Call management
    bool isCallActive(TalkgroupID talkgroup) const;
    ActiveCall* getActiveCall(TalkgroupID talkgroup);

    // Configuration
    void enableTalkgroup(TalkgroupID talkgroup, Priority priority = 5);
    void disableTalkgroup(TalkgroupID talkgroup);
    bool isTalkgroupEnabled(TalkgroupID talkgroup) const;

    void setTalkgroupPriority(TalkgroupID talkgroup, Priority priority);
    Priority getTalkgroupPriority(TalkgroupID talkgroup) const;

    // Statistics
    size_t getActiveCallCount() const;
    uint64_t getTotalCallCount() const { return total_calls_; }

private:
    void cleanupInactiveCalls();

    std::unique_ptr<AudioOutput> audio_output_;
    AudioConfig audio_config_;

    std::map<TalkgroupID, ActiveCall> active_calls_;
    std::map<TalkgroupID, Priority> talkgroup_priorities_;
    std::map<TalkgroupID, bool> enabled_talkgroups_;

    mutable std::mutex calls_mutex_;
    mutable std::mutex config_mutex_;

    uint64_t total_calls_;
    static constexpr uint64_t CALL_TIMEOUT_MS = 5000;  // 5 seconds
};

} // namespace TrunkSDR

#endif // CALL_MANAGER_H
