#include <rsans_whisper.h>

#include <rsans_data.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

// Writes a silent 16-bit PCM WAV file and returns its path
std::string writeTestWav(const std::string& name, uint16_t channels, uint32_t sampleRate) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    const uint32_t frameCount = sampleRate / 10; // 100 ms
    const uint16_t bitsPerSample = 16;
    const uint16_t blockAlign = channels * bitsPerSample / 8;
    const uint32_t dataSize = frameCount * blockAlign;

    std::ofstream out(path, std::ios::binary);
    const auto write32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
    const auto write16 = [&](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };

    out.write("RIFF", 4);
    write32(36 + dataSize);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write32(16);                       // fmt chunk size
    write16(1);                        // PCM
    write16(channels);
    write32(sampleRate);
    write32(sampleRate * blockAlign);  // byte rate
    write16(blockAlign);
    write16(bitsPerSample);
    out.write("data", 4);
    write32(dataSize);
    const std::vector<char> silence(dataSize, 0);
    out.write(silence.data(), static_cast<std::streamsize>(silence.size()));

    return path.string();
}

// Runs extractTokensFromAudio, expecting it to throw with a message containing expectedText
void expectExtractThrows(const std::string& audioPath,
                         const std::string& modelPath,
                         const std::string& expectedText) {
    try {
        extractTokensFromAudio(audioPath, modelPath);
        FAIL() << "Expected std::runtime_error containing: " << expectedText;
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find(expectedText), std::string::npos)
            << "Actual message: " << e.what();
    }
}

} // namespace

TEST(WhisperTest, ExtractTokensFromAudioThrowsGivenNonExistentAudioFile) {
    // Given
    const std::string audioPath = "/nonexistent/path/to/audio.wav";
    const std::string modelPath = "/nonexistent/path/to/model.bin";

    // When / Then
    expectExtractThrows(audioPath, modelPath, "Failed to open audio file");
}

TEST(WhisperTest, ExtractTokensFromAudioThrowsGivenEmptyPaths) {
    // Given
    const std::string audioPath = "";
    const std::string modelPath = "";

    // When / Then
    EXPECT_THROW(extractTokensFromAudio(audioPath, modelPath), std::runtime_error);
}

TEST(WhisperTest, ExtractTokensFromAudioThrowsGivenWrongSampleRate) {
    // Given: a 44.1 kHz WAV, the rate most music is distributed at
    const std::string audioPath = writeTestWav("rsans_test_44100.wav", 2, 44100);
    const std::string modelPath = "/nonexistent/path/to/model.bin";

    // When / Then
    expectExtractThrows(audioPath, modelPath, "got 44100 Hz");
    std::filesystem::remove(audioPath);
}

TEST(WhisperTest, ExtractTokensFromAudioThrowsGivenUnsupportedChannelCount) {
    // Given: a 16 kHz WAV with 3 channels
    const std::string audioPath = writeTestWav("rsans_test_3ch.wav", 3, 16000);
    const std::string modelPath = "/nonexistent/path/to/model.bin";

    // When / Then
    expectExtractThrows(audioPath, modelPath, "Unsupported number of audio channels: 3");
    std::filesystem::remove(audioPath);
}

TEST(WhisperTest, ExtractTokensFromAudioThrowsGivenNonExistentModel) {
    // Given: valid audio, but no model at the given path
    const std::string audioPath = writeTestWav("rsans_test_16000.wav", 1, 16000);
    const std::string modelPath = "/nonexistent/path/to/model.bin";

    // When / Then
    expectExtractThrows(audioPath, modelPath, "Failed to load Whisper model");
    std::filesystem::remove(audioPath);
}

TEST(WhisperTest, ExtractTokensFromAudioWritesNothingToStderrGivenNonExistentModel) {
    // Given: valid audio, so whisper.cpp runs and logs while failing to load the model
    const std::string audioPath = writeTestWav("rsans_test_log_stderr.wav", 1, 16000);
    const std::string modelPath = "/nonexistent/path/to/model.bin";

    // When
    testing::internal::CaptureStderr();
    EXPECT_THROW(extractTokensFromAudio(audioPath, modelPath), std::runtime_error);
    const std::string stderrOutput = testing::internal::GetCapturedStderr();

    // Then
    EXPECT_EQ(stderrOutput, "");
    std::filesystem::remove(audioPath);
}

TEST(WhisperTest, ExtractTokensFromAudioSendsLogLinesToHandlerGivenNonExistentModel) {
    // Given
    const std::string audioPath = writeTestWav("rsans_test_log_handler.wav", 1, 16000);
    const std::string modelPath = "/nonexistent/path/to/model.bin";
    std::vector<std::pair<WhisperLogLevel, std::string>> lines;
    const WhisperLogHandler onLog = [&](WhisperLogLevel level, const std::string& line) {
        lines.emplace_back(level, line);
    };

    // When
    EXPECT_THROW(extractTokensFromAudio(audioPath, modelPath, onLog), std::runtime_error);

    // Then: complete lines, and an error line naming the model
    ASSERT_FALSE(lines.empty());
    bool hasErrorAboutModel = false;
    for (const auto& [level, line] : lines) {
        EXPECT_EQ(line.find('\n'), std::string::npos) << "Line contains a newline: " << line;
        if (level == WhisperLogLevel::Error && line.find(modelPath) != std::string::npos) {
            hasErrorAboutModel = true;
        }
    }
    EXPECT_TRUE(hasErrorAboutModel);
    std::filesystem::remove(audioPath);
}

TEST(WhisperTest, ExtractTokensFromAudioIncludesWhisperOutputInErrorGivenNonExistentModel) {
    // Given
    const std::string audioPath = writeTestWav("rsans_test_log_error.wav", 1, 16000);
    const std::string modelPath = "/nonexistent/path/to/model.bin";

    // When / Then
    expectExtractThrows(audioPath, modelPath, "Last Whisper output:");
    std::filesystem::remove(audioPath);
}

TEST(MergeContractionsTest, MergeContractionsDoesNothingGivenEmptyVector) {
    // Given
    std::vector<Token> tokens;

    // When
    mergeContractions(tokens);

    // Then
    EXPECT_TRUE(tokens.empty());
}

TEST(MergeContractionsTest, MergeContractionsDoesNothingGivenSingleToken) {
    // Given
    std::vector<Token> tokens = {
        {0, "hello", 0, 500, 0, std::nullopt}
    };

    // When
    mergeContractions(tokens);

    // Then
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].text, "hello");
}

TEST(MergeContractionsTest, MergeContractionsMergesImGivenSeparateTokens) {
    // Given: "I" and "'m" as separate tokens
    std::vector<Token> tokens = {
        {0, "I", 0, 200, 0, std::nullopt},
        {1, "'m", 200, 400, 0, std::nullopt}
    };

    // When
    mergeContractions(tokens);

    // Then: Should be merged into "I'm"
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].text, "I'm");
    EXPECT_EQ(tokens[0].startMs, 0);
    EXPECT_EQ(tokens[0].endMs, 400);
    EXPECT_EQ(tokens[0].id, 0);
}

TEST(MergeContractionsTest, MergeContractionsMergesDontGivenSeparateTokens) {
    // Given: "don" and "'t" as separate tokens
    std::vector<Token> tokens = {
        {0, "don", 0, 200, 0, std::nullopt},
        {1, "'t", 200, 400, 0, std::nullopt}
    };

    // When
    mergeContractions(tokens);

    // Then: Should be merged into "don't"
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].text, "don't");
}

TEST(MergeContractionsTest, MergeContractionsMergesTheyreGivenSeparateTokens) {
    // Given: "they" and "'re" as separate tokens
    std::vector<Token> tokens = {
        {0, "they", 0, 300, 0, std::nullopt},
        {1, "'re", 300, 500, 0, std::nullopt}
    };

    // When
    mergeContractions(tokens);

    // Then
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].text, "they're");
}

TEST(MergeContractionsTest, MergeContractionsPreservesNonContractionsGivenNormalTokens) {
    // Given: Normal tokens without contractions
    std::vector<Token> tokens = {
        {0, "hello", 0, 300, 0, std::nullopt},
        {1, "world", 300, 600, 0, std::nullopt}
    };

    // When
    mergeContractions(tokens);

    // Then: Should remain unchanged
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].text, "hello");
    EXPECT_EQ(tokens[1].text, "world");
}

TEST(MergeContractionsTest, MergeContractionsHandlesMixedTokensGivenContractionsAndNormal) {
    // Given: Mix of contractions and normal words
    std::vector<Token> tokens = {
        {0, "I", 0, 100, 0, std::nullopt},
        {1, "'m", 100, 200, 0, std::nullopt},
        {2, "not", 200, 400, 0, std::nullopt},
        {3, "sure", 400, 600, 0, std::nullopt}
    };

    // When
    mergeContractions(tokens);

    // Then
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].text, "I'm");
    EXPECT_EQ(tokens[1].text, "not");
    EXPECT_EQ(tokens[2].text, "sure");
}

TEST(MergeContractionsTest, MergeContractionsHandlesMultipleContractionsGivenSentence) {
    // Given: Sentence with multiple contractions
    std::vector<Token> tokens = {
        {0, "I", 0, 100, 0, std::nullopt},
        {1, "'m", 100, 200, 0, std::nullopt},
        {2, "sure", 200, 400, 0, std::nullopt},
        {3, "they", 400, 600, 0, std::nullopt},
        {4, "'ll", 600, 800, 0, std::nullopt},
        {5, "come", 800, 1000, 0, std::nullopt}
    };

    // When
    mergeContractions(tokens);

    // Then
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0].text, "I'm");
    EXPECT_EQ(tokens[1].text, "sure");
    EXPECT_EQ(tokens[2].text, "they'll");
    EXPECT_EQ(tokens[3].text, "come");
}

TEST(MergeContractionsTest, MergeContractionsReassignsIdsGivenMergedTokens) {
    // Given: Tokens that will be merged
    std::vector<Token> tokens = {
        {0, "I", 0, 100, 0, std::nullopt},
        {1, "'m", 100, 200, 0, std::nullopt},
        {2, "here", 200, 400, 0, std::nullopt}
    };

    // When
    mergeContractions(tokens);

    // Then: IDs should be sequential
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].id, 0);
    EXPECT_EQ(tokens[1].id, 1);
}

TEST(MergeContractionsTest, MergeContractionsPreservesLineIndexGivenMergedTokens) {
    // Given: Tokens on different lines
    std::vector<Token> tokens = {
        {0, "I", 0, 100, 0, std::nullopt},
        {1, "'m", 100, 200, 0, std::nullopt},
        {2, "they", 500, 600, 1, std::nullopt},
        {3, "'re", 600, 700, 1, std::nullopt}
    };

    // When
    mergeContractions(tokens);

    // Then: Line indices should be preserved from first token
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].lineIndex, 0);
    EXPECT_EQ(tokens[1].lineIndex, 1);
}

TEST(MergeContractionsTest, MergeContractionsHandlesApostropheWordGivenNonContraction) {
    // Given: A word that starts with apostrophe but is standalone (like 'bout)
    std::vector<Token> tokens = {
        {0, "'bout", 0, 300, 0, std::nullopt},
        {1, "time", 300, 500, 0, std::nullopt}
    };

    // When
    mergeContractions(tokens);

    // Then: Should remain separate (no preceding word to merge with)
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].text, "'bout");
    EXPECT_EQ(tokens[1].text, "time");
}

TEST(MergeContractionsTest, MergeContractionsHandlesContractionAtEndGivenLastTwoTokens) {
    // Given: Contraction at the end of token list
    std::vector<Token> tokens = {
        {0, "yes", 0, 200, 0, std::nullopt},
        {1, "I", 200, 300, 0, std::nullopt},
        {2, "'m", 300, 400, 0, std::nullopt}
    };

    // When
    mergeContractions(tokens);

    // Then
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].text, "yes");
    EXPECT_EQ(tokens[1].text, "I'm");
}
