#include <rsans_whisper.h>

#include <rsans_data.h>

#include <whisper.h>

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

std::vector<float> loadAudioFile(const std::string& audioPath) {
    std::vector<float> audioData;

    drwav wav;
    if (!drwav_init_file(&wav, audioPath.c_str(), nullptr)) {
        fprintf(stderr, "Failed to open audio file: %s\n", audioPath.c_str());
        return audioData;
    }

    if (wav.channels != 1 && wav.channels != 2) {
        fprintf(stderr, "Unsupported number of channels: %u\n", wav.channels);
        drwav_uninit(&wav);
        return audioData;
    }

    // TODO: sample rate caps @ 16kHz, will need to change logic to allow resampling 
    if (wav.sampleRate != WHISPER_SAMPLE_RATE) {
        fprintf(stderr, "Audio sample rate must be %d Hz, got %u Hz\n",
                WHISPER_SAMPLE_RATE, wav.sampleRate);
        drwav_uninit(&wav);
        return audioData;
    }

    const size_t sampleCount = wav.totalPCMFrameCount;
    std::vector<int16_t> samples(sampleCount * wav.channels);

    const size_t framesRead = drwav_read_pcm_frames_s16(&wav, sampleCount, samples.data());
    drwav_uninit(&wav);

    if (framesRead != sampleCount) {
        fprintf(stderr, "Failed to read all audio frames\n");
        return audioData;
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

void tokenizeAudio(ProjectData& project) {  // TODO: Expand logic to choose active model for this call
    project.tokens = extractTokensFromAudio(
        project.audio.path,
        project.model.base
    );
}

std::vector<Token> extractTokensFromAudio(
    const std::string& audioPath,
    const std::string& modelPath
) {
    std::vector<Token> tokens;

    const std::vector<float> audioData = loadAudioFile(audioPath);
    if (audioData.empty()) {
        fprintf(stderr, "Failed to load audio data\n");
        return tokens;
    }

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;
    cparams.dtw_token_timestamps = true;

    whisper_context* ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
    if (!ctx) {
        fprintf(stderr, "Failed to initialize Whisper context from model: %s\n",
                modelPath.c_str());
        return tokens;
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
        fprintf(stderr, "Failed to process audio\n");
        whisper_free(ctx);
        return tokens;
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
