#include <rsans_audio_player.h>

#include <miniaudio.h>
#include <sndfile.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

struct AudioPlayer::Impl {
    Output output;
    State state = State::Closed;
    std::string path;

    // The open file. The sound device's thread reads it, so it's only
    // changed (seeked, closed) while holding fileMutex.
    SNDFILE* file = nullptr;
    SF_INFO info{};
    std::mutex fileMutex;
    std::atomic<std::uint64_t> cursor{0};  // Frames handed to the device
    std::atomic<bool> finished{false};     // The device has read to the end

    ma_context context{};
    bool contextReady = false;
    ma_device device{};
    bool deviceReady = false;

    explicit Impl(Output out) : output(out) {}

    // Runs on the sound device's thread: fills `out` with the next frames.
    // The buffer starts silent, so frames that aren't read stay silent.
    static void onData(ma_device* device, void* out, const void* /*in*/, ma_uint32 frameCount) {
        Impl* self = static_cast<Impl*>(device->pUserData);
        // Never wait here: if the UI thread is seeking, skip this buffer
        const std::unique_lock<std::mutex> lock(self->fileMutex, std::try_to_lock);
        if (!lock.owns_lock() || self->file == nullptr || self->finished) {
            return;
        }
        const sf_count_t read = sf_readf_float(self->file, static_cast<float*>(out), frameCount);
        if (read > 0) {
            self->cursor += static_cast<std::uint64_t>(read);
        }
        if (read < static_cast<sf_count_t>(frameCount)) {
            self->finished = true;
        }
    }

    std::string startDevice() {
        if (!contextReady) {
            // Silent: miniaudio's null backend, which plays at the real pace
            const std::array<ma_backend, 1> silent{ma_backend_null};
            const ma_result result = output == Output::Silent
                ? ma_context_init(silent.data(), static_cast<ma_uint32>(silent.size()), nullptr, &context)
                : ma_context_init(nullptr, 0, nullptr, &context);
            if (result != MA_SUCCESS) {
                return std::string("Can't use the sound system: ") + ma_result_description(result);
            }
            contextReady = true;
        }
        if (!deviceReady) {
            // In the file's own format; miniaudio converts it for the device
            ma_device_config config = ma_device_config_init(ma_device_type_playback);
            config.playback.format = ma_format_f32;
            config.playback.channels = static_cast<ma_uint32>(info.channels);
            config.sampleRate = static_cast<ma_uint32>(info.samplerate);
            config.dataCallback = onData;
            config.pUserData = this;
            const ma_result result = ma_device_init(&context, &config, &device);
            if (result != MA_SUCCESS) {
                return std::string("Can't open the sound device: ") + ma_result_description(result);
            }
            deviceReady = true;
        }
        const ma_result result = ma_device_start(&device);
        if (result != MA_SUCCESS) {
            return std::string("Can't start the sound device: ") + ma_result_description(result);
        }
        return "";
    }

    void stopDevice() {
        if (deviceReady && ma_device_is_started(&device) == MA_TRUE) {
            ma_device_stop(&device);  // Waits for the device's thread to finish its buffer
        }
    }

    void rewind() {
        const std::lock_guard<std::mutex> lock(fileMutex);
        if (file != nullptr) {
            sf_seek(file, 0, SEEK_SET);
        }
        cursor = 0;
        finished = false;
    }

    // Moves to a frame (stopped, so the device's thread isn't reading).
    // False if the file can't seek there.
    bool seek(std::uint64_t frame) {
        const std::lock_guard<std::mutex> lock(fileMutex);
        if (file == nullptr || sf_seek(file, static_cast<sf_count_t>(frame), SEEK_SET) < 0) {
            return false;
        }
        cursor = frame;
        finished = false;
        return true;
    }

    void close() {
        if (deviceReady) {
            ma_device_uninit(&device);  // Stops it first
            deviceReady = false;
        }
        const std::lock_guard<std::mutex> lock(fileMutex);
        if (file != nullptr) {
            sf_close(file);
            file = nullptr;
        }
        info = SF_INFO{};
        cursor = 0;
        finished = false;
        path.clear();
        state = State::Closed;
    }
};

AudioPlayer::AudioPlayer(Output output) : d_impl(std::make_unique<Impl>(output)) {}

AudioPlayer::~AudioPlayer() {
    d_impl->close();
    if (d_impl->contextReady) {
        ma_context_uninit(&d_impl->context);
    }
}

std::string AudioPlayer::open(const std::string& path) {
    close();
    if (path.empty()) {
        return "No audio file";
    }
    SF_INFO info{};
    SNDFILE* file = sf_open(path.c_str(), SFM_READ, &info);
    if (file == nullptr) {
        return "Can't open the audio: " + std::string(sf_strerror(nullptr));
    }
    if (info.channels <= 0 || info.samplerate <= 0) {
        sf_close(file);
        return "Can't open the audio: it has no sound";
    }
    {
        const std::lock_guard<std::mutex> lock(d_impl->fileMutex);
        d_impl->file = file;
        d_impl->info = info;
    }
    d_impl->path = path;
    d_impl->state = State::Stopped;
    return "";
}

void AudioPlayer::close() {
    d_impl->close();
}

const std::string& AudioPlayer::path() const {
    return d_impl->path;
}

std::string AudioPlayer::play() {
    const State current = state();
    if (current == State::Closed || current == State::Playing) {
        return "";
    }
    const std::string error = d_impl->startDevice();
    if (!error.empty()) {
        return error;
    }
    d_impl->state = State::Playing;
    return "";
}

std::string AudioPlayer::playFrom(int ms) {
    if (state() == State::Closed) {
        return "";
    }
    // The frame at ms, kept inside the file
    const auto rate = static_cast<std::uint64_t>(d_impl->info.samplerate);
    const auto frames = static_cast<std::uint64_t>(std::max<sf_count_t>(d_impl->info.frames, 0));
    std::uint64_t frame = static_cast<std::uint64_t>(std::max(ms, 0)) * rate / 1000;
    if (frames > 0) {
        frame = std::min(frame, frames - 1);
    }

    d_impl->stopDevice();
    if (!d_impl->seek(frame)) {
        d_impl->rewind();
        d_impl->state = State::Stopped;
        return "Can't play from " + std::to_string(ms / 1000) + " s: the audio can't be searched";
    }
    d_impl->state = State::Paused;  // Positioned, not yet playing
    return play();
}

void AudioPlayer::pause() {
    if (state() == State::Playing) {
        d_impl->stopDevice();
        d_impl->state = State::Paused;
    }
}

void AudioPlayer::stop() {
    if (state() == State::Closed) {
        return;
    }
    d_impl->stopDevice();
    d_impl->rewind();
    d_impl->state = State::Stopped;
}

AudioPlayer::State AudioPlayer::state() {
    if (d_impl->state == State::Playing && d_impl->finished) {
        d_impl->stopDevice();
        d_impl->rewind();
        d_impl->state = State::Stopped;
    }
    return d_impl->state;
}

int AudioPlayer::positionMs() const {
    if (d_impl->info.samplerate <= 0) {
        return 0;
    }
    return static_cast<int>(d_impl->cursor.load() * 1000 / static_cast<std::uint64_t>(d_impl->info.samplerate));
}

int AudioPlayer::lengthMs() const {
    if (d_impl->info.samplerate <= 0 || d_impl->info.frames <= 0) {
        return 0;
    }
    return static_cast<int>(d_impl->info.frames * 1000 / d_impl->info.samplerate);
}
