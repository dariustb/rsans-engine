#include <rsans_whisper.h>

#include <rsans_data.h>

#include <whisper.h>

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <cstddef>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Routes whisper.cpp and ggml logging to a handler for as long as it exists,
// then restores the default (stderr). ggml sends a message in pieces, so text
// is buffered until a full line arrives. The last few lines are kept so
// errors can include them.
class WhisperLogCapture {
  private:
    static constexpr std::size_t maxRecentLines = 10;

    const WhisperLogHandler& d_onLog;
    std::string d_partialLine;
    WhisperLogLevel d_lastLevel = WhisperLogLevel::Info;
    std::deque<std::string> d_recentLines;

    static WhisperLogLevel toLogLevel(ggml_log_level level, WhisperLogLevel previous) {
        switch (level) {
        case GGML_LOG_LEVEL_DEBUG: return WhisperLogLevel::Debug;
        case GGML_LOG_LEVEL_WARN:  return WhisperLogLevel::Warn;
        case GGML_LOG_LEVEL_ERROR: return WhisperLogLevel::Error;
        case GGML_LOG_LEVEL_CONT:  return previous;
        default:                   return WhisperLogLevel::Info;
        }
    }

    static void callback(ggml_log_level level, const char* text, void* userData) {
        auto* self = static_cast<WhisperLogCapture*>(userData);
        if (self == nullptr || text == nullptr) {
            return;
        }
        self->d_lastLevel = toLogLevel(level, self->d_lastLevel);
        for (const char* c = text; *c != '\0'; ++c) {
            if (*c == '\n') {
                self->emitLine();
            } else {
                self->d_partialLine += *c;
            }
        }
    }

    void emitLine() {
        if (d_partialLine.empty()) {
            return;
        }
        d_recentLines.push_back(d_partialLine);
        if (d_recentLines.size() > maxRecentLines) {
            d_recentLines.pop_front();
        }
        if (d_onLog) {
            // An exception must not unwind through whisper's C code
            try {
                d_onLog(d_lastLevel, d_partialLine);
            } catch (...) {
            }
        }
        d_partialLine.clear();
    }

  public:
    explicit WhisperLogCapture(const WhisperLogHandler& onLog)
    : d_onLog(onLog) {
        whisper_log_set(&WhisperLogCapture::callback, this);
    }

    ~WhisperLogCapture() {
        whisper_log_set(nullptr, nullptr);
    }

    WhisperLogCapture(const WhisperLogCapture&) = delete;
    WhisperLogCapture& operator=(const WhisperLogCapture&) = delete;
    WhisperLogCapture(WhisperLogCapture&&) = delete;
    WhisperLogCapture& operator=(WhisperLogCapture&&) = delete;

    // Appends the most recent log lines to an error message
    std::string withRecentOutput(const std::string& message) {
        emitLine();
        if (d_recentLines.empty()) {
            return message;
        }
        std::string result = message + "\nLast Whisper output:";
        for (const std::string& line : d_recentLines) {
            result += "\n  " + line;
        }
        return result;
    }
};

std::vector<float> loadAudioFile(const std::string& audioPath) {
    std::vector<float> audioData;

    drwav wav;
    if (!drwav_init_file(&wav, audioPath.c_str(), nullptr)) {
        throw std::runtime_error("Failed to open audio file (must be a WAV file): " + audioPath);
    }

    if (wav.channels != 1 && wav.channels != 2) {
        const unsigned int channels = wav.channels;
        drwav_uninit(&wav);
        throw std::runtime_error("Unsupported number of audio channels: " +
                                 std::to_string(channels) + " (must be mono or stereo)");
    }

    // TODO: sample rate caps @ 16kHz, will need to change logic to allow resampling 
    if (wav.sampleRate != WHISPER_SAMPLE_RATE) {
        const unsigned int sampleRate = wav.sampleRate;
        drwav_uninit(&wav);
        throw std::runtime_error("Audio sample rate must be " +
                                 std::to_string(WHISPER_SAMPLE_RATE) + " Hz, got " +
                                 std::to_string(sampleRate) + " Hz");
    }

    const size_t sampleCount = wav.totalPCMFrameCount;
    std::vector<int16_t> samples(sampleCount * wav.channels);

    const size_t framesRead = drwav_read_pcm_frames_s16(&wav, sampleCount, samples.data());
    drwav_uninit(&wav);

    if (framesRead != sampleCount) {
        throw std::runtime_error("Failed to read all audio frames from: " + audioPath);
    }

    audioData.resize(sampleCount);

    if (wav.channels == 1) {
        for (size_t i = 0; i < sampleCount; ++i) {
            audioData[i] = static_cast<float>(samples[i]) / 32768.0f;
        }
    } else {
        for (size_t i = 0; i < sampleCount; ++i) {
            const float left = static_cast<float>(samples[i * 2]) / 32768.0f;
            const float right = static_cast<float>(samples[i * 2 + 1]) / 32768.0f;
            audioData[i] = (left + right) / 2.0f;
        }
    }

    return audioData;
}

}

ProjectData tokenizeAudio(const ProjectData& project,
                          const WhisperLogHandler& onLog) {
    std::vector<Token> tokens = extractTokensFromAudio(
        project.audio.path,
        project.model.base,
        onLog
    );
    ProjectData result(project.toJson());
    return ProjectData(std::move(result), tokens);
}

std::vector<Token> extractTokensFromAudio(
    const std::string& audioPath,
    const std::string& modelPath,
    const WhisperLogHandler& onLog
) {
    std::vector<Token> tokens;

    const std::vector<float> audioData = loadAudioFile(audioPath);
    if (audioData.empty()) {
        throw std::runtime_error("Audio file contains no samples: " + audioPath);
    }

    WhisperLogCapture logCapture(onLog);

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;
    cparams.dtw_token_timestamps = true;

    whisper_context* ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
    if (!ctx) {
        throw std::runtime_error(
            logCapture.withRecentOutput("Failed to load Whisper model: " + modelPath));
    }

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress = false;
    wparams.print_special = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = false;
    wparams.token_timestamps = true;
    wparams.max_len = 0; // 0 disables the limit
    wparams.language = "en";

    if (whisper_full(ctx, wparams, audioData.data(), audioData.size()) != 0) {
        whisper_free(ctx);
        throw std::runtime_error(
            logCapture.withRecentOutput("Whisper failed to transcribe audio: " + audioPath));
    }

    const int nSegments = whisper_full_n_segments(ctx);
    int tokenId = 0;
    int lineIndex = 0;

    for (int i = 0; i < nSegments; ++i) {
        const int nTokens = whisper_full_n_tokens(ctx, i);

        for (int j = 0; j < nTokens; ++j) {
            const whisper_token_data tokenData = whisper_full_get_token_data(ctx, i, j);
            const char* text = whisper_full_get_token_text(ctx, i, j);

            if (!text || text[0] == '\0') {
                continue;
            }

            std::string tokenText(text);

            tokenText.erase(0, tokenText.find_first_not_of(" \t\n\r"));
            tokenText.erase(tokenText.find_last_not_of(" \t\n\r") + 1);

            // This skips tokens with non-word text that Whisper creates sometimes
            if (tokenText.empty() || tokenText == "[_BEG_]" ||
                tokenText.rfind("[_TT_", 0) == 0 || tokenText[0] == '<') {
                continue;
            }

            const Token token(
                tokenId++,
                tokenText,
                static_cast<int>(tokenData.t0 * 10),
                static_cast<int>(tokenData.t1 * 10),
                lineIndex,
                std::nullopt
            );

            tokens.push_back(token);
        }

        ++lineIndex;
    }

    whisper_free(ctx);

    mergeContractions(tokens);

    // RSANS is for sung or spoken lyrics, so audio without words is an error
    if (tokens.empty()) {
        throw std::runtime_error("No words were detected in the audio: " + audioPath);
    }

    return tokens;
}

void mergeContractions(std::vector<Token>& tokens) {
    if (tokens.size() < 2) {
        return;
    }

    std::vector<Token> merged;
    merged.reserve(tokens.size());

    size_t i = 0;
    while (i < tokens.size()) {
        Token current = tokens[i];

        // Check if next token starts with apostrophe (contraction suffix)
        if (i + 1 < tokens.size() && !tokens[i + 1].text.empty() &&
            tokens[i + 1].text[0] == '\'') {
            // Merge: concatenate text, use first start and second end
            current.text += tokens[i + 1].text;
            current.endMs = tokens[i + 1].endMs;
            i += 2; // Skip both tokens
        } else {
            i += 1;
        }

        merged.push_back(current);
    }

    // Reassign sequential IDs
    for (size_t j = 0; j < merged.size(); ++j) {
        merged[j].id = static_cast<int>(j);
    }

    tokens = std::move(merged);
}
