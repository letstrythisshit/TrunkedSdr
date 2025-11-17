#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include "../utils/types.h"
#include <pulse/simple.h>
#include <pulse/error.h>
#include <atomic>
#include <queue>
#include <mutex>
#include <thread>

namespace TrunkSDR {

class AudioOutput {
public:
    AudioOutput();
    ~AudioOutput();

    bool initialize(const std::string& device_name = "default",
                   uint32_t sample_rate = AUDIO_SAMPLE_RATE);
    bool start();
    bool stop();

    void playAudio(const AudioBuffer& buffer);
    void queueAudio(const AudioFrame& frame);

    bool isPlaying() const { return playing_; }

    void setVolume(float volume);  // 0.0 to 1.0
    float getVolume() const { return volume_; }

private:
    void playbackThread();
    void processQueue();

    pa_simple* pa_stream_;
    std::atomic<bool> running_;
    std::atomic<bool> playing_;

    uint32_t sample_rate_;
    float volume_;

    std::queue<AudioFrame> audio_queue_;
    std::mutex queue_mutex_;
    std::thread playback_thread_;

    AudioBuffer temp_buffer_;
};

} // namespace TrunkSDR

#endif // AUDIO_OUTPUT_H
