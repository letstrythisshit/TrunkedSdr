#include "audio_output.h"
#include "../utils/logger.h"
#include <cstring>

namespace TrunkSDR {

AudioOutput::AudioOutput()
    : pa_stream_(nullptr)
    , running_(false)
    , playing_(false)
    , sample_rate_(AUDIO_SAMPLE_RATE)
    , volume_(1.0f) {
}

AudioOutput::~AudioOutput() {
    stop();

    if (pa_stream_) {
        pa_simple_free(pa_stream_);
        pa_stream_ = nullptr;
    }
}

bool AudioOutput::initialize(const std::string& device_name, uint32_t sample_rate) {
    sample_rate_ = sample_rate;

    // PulseAudio sample spec
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;  // 16-bit signed little-endian
    ss.channels = 1;              // Mono
    ss.rate = sample_rate;

    int error;
    pa_stream_ = pa_simple_new(
        nullptr,                      // Default server
        "TrunkSDR",                   // Application name
        PA_STREAM_PLAYBACK,           // Playback stream
        device_name.c_str(),          // Device name
        "Radio Audio",                // Stream description
        &ss,                          // Sample spec
        nullptr,                      // Default channel map
        nullptr,                      // Default buffering attributes
        &error                        // Error code
    );

    if (!pa_stream_) {
        LOG_ERROR("PulseAudio initialization failed:", pa_strerror(error));
        return false;
    }

    LOG_INFO("Audio output initialized: rate =", sample_rate, "Hz");
    return true;
}

bool AudioOutput::start() {
    if (running_) {
        return true;
    }

    running_ = true;
    playback_thread_ = std::thread(&AudioOutput::playbackThread, this);

    LOG_INFO("Audio output started");
    return true;
}

bool AudioOutput::stop() {
    if (!running_) {
        return true;
    }

    running_ = false;

    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }

    LOG_INFO("Audio output stopped");
    return true;
}

void AudioOutput::playAudio(const AudioBuffer& buffer) {
    if (!pa_stream_ || buffer.empty()) {
        return;
    }

    // Apply volume
    temp_buffer_ = buffer;
    for (auto& sample : temp_buffer_) {
        sample = static_cast<AudioSample>(sample * volume_);
    }

    // Write to PulseAudio
    int error;
    if (pa_simple_write(pa_stream_,
                       temp_buffer_.data(),
                       temp_buffer_.size() * sizeof(AudioSample),
                       &error) < 0) {
        LOG_ERROR("PulseAudio write failed:", pa_strerror(error));
    }

    playing_ = true;
}

void AudioOutput::queueAudio(const AudioFrame& frame) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    audio_queue_.push(frame);
}

void AudioOutput::playbackThread() {
    LOG_INFO("Playback thread started");

    while (running_) {
        processQueue();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    LOG_INFO("Playback thread stopped");
}

void AudioOutput::processQueue() {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (audio_queue_.empty()) {
        playing_ = false;
        return;
    }

    // Get next frame
    AudioFrame frame = audio_queue_.front();
    audio_queue_.pop();

    // Play it
    playAudio(frame.samples);

    LOG_DEBUG("Playing audio: TG =", frame.talkgroup,
              "samples =", frame.samples.size());
}

void AudioOutput::setVolume(float volume) {
    volume_ = std::max(0.0f, std::min(1.0f, volume));
}

} // namespace TrunkSDR
