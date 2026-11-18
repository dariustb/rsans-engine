#include <rsans_whisper.h>

#include <rsans_data.h>
#include <rsans_process.h>
#include <rsans_test_utils.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

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

TEST(WhisperTest, ExtractTokensFromAudioThrowsCancelledGivenCancelAlreadyRequested) {
    // Given: valid audio and a missing model, so only cancellation explains the error
    const std::string audioPath = writeTestWav("rsans_test_cancel.wav", 1, 16000);
    const std::string modelPath = "/nonexistent/path/to/model.bin";
    CancelFlag cancel;
    cancel.request();

    // When / Then
    EXPECT_THROW(extractTokensFromAudio(audioPath, modelPath, {}, {}, &cancel), CancelledError);
    std::filesystem::remove(audioPath);
}

TEST(WhisperTest, ExtractTokensFromAudioDoesNotReportProgressGivenFailedModelLoad) {
    // Given
    const std::string audioPath = writeTestWav("rsans_test_progress.wav", 1, 16000);
    const std::string modelPath = "/nonexistent/path/to/model.bin";
    bool progressReported = false;
    const ProgressHandler onProgress = [&](double) { progressReported = true; };

    // When
    EXPECT_THROW(extractTokensFromAudio(audioPath, modelPath, {}, onProgress), std::runtime_error);

    // Then
    EXPECT_FALSE(progressReported);
    std::filesystem::remove(audioPath);
}

// Audio conversion for Whisper

namespace {

const std::string missingModel = "/nonexistent/path/to/model.bin";

// Counts temp files left by tokenizeAudio's conversion
std::size_t countConversionTempFiles() {
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::temp_directory_path())) {
        if (entry.path().filename().string().rfind("rsans-whisper-", 0) == 0) {
            ++count;
        }
    }
    return count;
}

ProjectData projectWithAudio(const std::string& audioPath) {
    ProjectData project;
    project.audio.path = audioPath;
    project.model.base = missingModel;
    return project;
}

} // namespace

TEST(WhisperAudioTest, IsWhisperReadyAudioAcceptsOnly16kHzMonoOrStereoWav) {
    // Given
    const std::string mono16k = writeTestWav("rsans_ready_mono.wav", 1, 16000);
    const std::string stereo16k = writeTestWav("rsans_ready_stereo.wav", 2, 16000);
    const std::string stereo44k = writeTestWav("rsans_ready_44k.wav", 2, 44100);
    const std::string threeChannel = writeTestWav("rsans_ready_3ch.wav", 3, 16000);
    const std::string notAudio = (std::filesystem::temp_directory_path() / "rsans_ready_text.wav").string();
    std::ofstream(notAudio) << "not audio";

    // Then
    EXPECT_TRUE(isWhisperReadyAudio(mono16k));
    EXPECT_TRUE(isWhisperReadyAudio(stereo16k));
    EXPECT_FALSE(isWhisperReadyAudio(stereo44k));
    EXPECT_FALSE(isWhisperReadyAudio(threeChannel));
    EXPECT_FALSE(isWhisperReadyAudio(notAudio));
    EXPECT_FALSE(isWhisperReadyAudio("/nonexistent/audio.wav"));

    for (const std::string& path : {mono16k, stereo16k, stereo44k, threeChannel, notAudio}) {
        std::filesystem::remove(path);
    }
}

TEST(WhisperAudioTest, ConvertAudioForWhisperProduces16kHzMonoWavGiven44kStereoWav) {
    if (!ffmpegAvailable()) {
        GTEST_SKIP() << "ffmpeg not found";
    }
    // Given: 2 seconds at 44.1 kHz stereo
    const std::string input = writeTestWav("rsans_convert_in.wav", 2, 44100, 2000);
    const std::string output = (std::filesystem::temp_directory_path() / "rsans_convert_out.wav").string();

    // When
    convertAudioForWhisper(input, output, "ffmpeg");

    // Then
    EXPECT_TRUE(isWhisperReadyAudio(output));
    EXPECT_NEAR(getAudioDuration(output), 2.0, 0.01);
    std::filesystem::remove(input);
    std::filesystem::remove(output);
}

TEST(WhisperAudioTest, ConvertAudioForWhisperProducesWavGivenFlac) {
    if (!ffmpegAvailable()) {
        GTEST_SKIP() << "ffmpeg not found";
    }
    // Given: a compressed format libsndfile and dr_wav aren't asked to read
    const std::filesystem::path dir = std::filesystem::temp_directory_path();
    const std::string flac = (dir / "rsans convert in.flac").string();
    ASSERT_EQ(runProcess({"ffmpeg", "-nostdin", "-y", "-loglevel", "error",
                          "-f", "lavfi", "-i", "sine=frequency=440:duration=1:sample_rate=48000",
                          flac}), 0);
    const std::string output = (dir / "rsans convert out.wav").string();

    // When
    convertAudioForWhisper(flac, output, "ffmpeg");

    // Then
    EXPECT_TRUE(isWhisperReadyAudio(output));
    EXPECT_NEAR(getAudioDuration(output), 1.0, 0.01);
    std::filesystem::remove(flac);
    std::filesystem::remove(output);
}

TEST(WhisperAudioTest, ConvertAudioForWhisperThrowsWithFfmpegErrorGivenNonAudioFile) {
    if (!ffmpegAvailable()) {
        GTEST_SKIP() << "ffmpeg not found";
    }
    // Given
    const std::filesystem::path dir = std::filesystem::temp_directory_path();
    const std::string input = (dir / "rsans_convert_bad.mp3").string();
    std::ofstream(input) << "not audio";
    const std::string output = (dir / "rsans_convert_bad_out.wav").string();

    // When / Then
    try {
        convertAudioForWhisper(input, output, "ffmpeg");
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("Failed to convert audio"), std::string::npos) << message;
        EXPECT_NE(message.find('\n'), std::string::npos) << "No ffmpeg output in: " << message;
    }
    std::filesystem::remove(input);
    std::filesystem::remove(output);
}

TEST(WhisperAudioTest, ConvertAudioForWhisperThrowsCancelledGivenCancelAlreadyRequested) {
    // Given
    const std::string input = writeTestWav("rsans_convert_cancel.wav", 2, 44100);
    CancelFlag cancel;
    cancel.request();

    // When / Then
    EXPECT_THROW(convertAudioForWhisper(input, input + ".out.wav", "ffmpeg", &cancel), CancelledError);
    std::filesystem::remove(input);
}

TEST(WhisperAudioTest, TokenizeAudioConvertsAudioGiven44kWav) {
    if (!ffmpegAvailable()) {
        GTEST_SKIP() << "ffmpeg not found";
    }
    // Given: audio that used to be rejected outright
    const std::string audio = writeTestWav("rsans_tokenize_44k.wav", 2, 44100);
    const std::size_t tempFilesBefore = countConversionTempFiles();

    // When
    std::string message;
    try {
        tokenizeAudio(projectWithAudio(audio), "ffmpeg");
    } catch (const std::runtime_error& e) {
        message = e.what();
    }

    // Then: conversion succeeded, so the failure is now the missing model
    EXPECT_NE(message.find("Failed to load Whisper model"), std::string::npos) << message;

    // And the converted temp file is deleted
    EXPECT_EQ(countConversionTempFiles(), tempFilesBefore);
    std::filesystem::remove(audio);
}

TEST(WhisperAudioTest, TokenizeAudioSkipsConversionGiven16kWav) {
    // Given: ffmpeg path that can't run, which is fine because none is needed
    const std::string audio = writeTestWav("rsans_tokenize_16k.wav", 1, 16000);

    // When / Then
    try {
        tokenizeAudio(projectWithAudio(audio), "/nonexistent/ffmpeg");
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Failed to load Whisper model"), std::string::npos) << e.what();
    }
    std::filesystem::remove(audio);
}

TEST(WhisperAudioTest, TokenizeAudioThrowsGivenUnconvertibleAudioAndMissingFfmpeg) {
    // Given
    const std::string audio = writeTestWav("rsans_tokenize_noffmpeg.wav", 2, 44100);

    // When / Then
    try {
        tokenizeAudio(projectWithAudio(audio), "/nonexistent/ffmpeg");
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("/nonexistent/ffmpeg"), std::string::npos) << e.what();
    }
    std::filesystem::remove(audio);
}

// Whisper's tokens into words

namespace {

TokenPiece piece(const std::string& text, int startMs, int endMs, int segment = 0, bool special = false) {
    return TokenPiece{text, startMs, endMs, segment, special};
}

std::vector<std::string> textsOf(const std::vector<Token>& tokens) {
    std::vector<std::string> texts;
    for (const Token& token : tokens) {
        texts.push_back(token.text);
    }
    return texts;
}

} // namespace

TEST(WordsFromPiecesTest, JoinsSplitWordsAndKeepsPunctuationOnWord) {
    // Real pieces from base.en on a song: " R" "iddle", " Mon" "a" " Lisa" ","
    const std::vector<Token> words = wordsFromPieces({
        piece(" R", 0, 100), piece("iddle", 100, 300), piece(" me", 300, 400),
        piece(" Mon", 500, 600), piece("a", 600, 700), piece(" Lisa", 700, 900), piece(",", 900, 950),
        piece(" pepper", 1000, 1200), piece("oni", 1200, 1500),
    });

    EXPECT_EQ(textsOf(words), (std::vector<std::string>{"Riddle", "me", "Mona", "Lisa,", "pepperoni"}));
    // A word runs from its first piece to its last
    EXPECT_EQ(words[0].startMs, 0);
    EXPECT_EQ(words[0].endMs, 300);
    EXPECT_EQ(words[4].startMs, 1000);
    EXPECT_EQ(words[4].endMs, 1500);
    for (std::size_t i = 0; i < words.size(); ++i) {
        EXPECT_EQ(words[i].id, static_cast<int>(i));
    }
}

TEST(WordsFromPiecesTest, RejoinsSplitCharacterAndDropsMusicNotes) {
    // ♪ (E2 99 AA) arrives as two pieces of raw bytes, around the words
    const std::vector<Token> words = wordsFromPieces({
        piece("[_BEG_]", 0, 0, 0, true),
        piece(" \xE2\x99", 0, 100), piece("\xAA", 100, 200),
        piece(" Feel", 200, 400), piece(" my", 400, 500), piece(" heart", 500, 800),
        piece(" \xE2\x99", 800, 900), piece("\xAA", 900, 1000),
        piece("[_TT_136]", 1000, 1000, 0, true),
    });

    EXPECT_EQ(textsOf(words), (std::vector<std::string>{"Feel", "my", "heart"}));
}

TEST(WordsFromPiecesTest, KeepsContractionsTogether) {
    const std::vector<Token> words = wordsFromPieces({
        piece(" don", 0, 200), piece("'t", 200, 300), piece(" stop", 300, 600),
    });

    EXPECT_EQ(textsOf(words), (std::vector<std::string>{"don't", "stop"}));
    EXPECT_EQ(words[0].endMs, 300);
}

TEST(WordsFromPiecesTest, SegmentsAndSpecialTokensEndWords) {
    const std::vector<Token> words = wordsFromPieces({
        piece(" walk", 0, 200, 0), piece("[_TT_50]", 200, 200, 0, true),
        piece("ing", 300, 400, 0),  // After a timestamp: a word of its own
        piece("home", 500, 700, 1), // A new segment without a space: still a new word
    });

    EXPECT_EQ(textsOf(words), (std::vector<std::string>{"walk", "ing", "home"}));
}

TEST(WordsFromPiecesTest, LinesCountOnlySegmentsWithWords) {
    // Segment 1 is only music
    const std::vector<Token> words = wordsFromPieces({
        piece(" one", 0, 100, 0),
        piece(" \xE2\x99", 200, 300, 1), piece("\xAA", 300, 400, 1),
        piece(" two", 500, 600, 2), piece(" three", 600, 700, 2),
    });

    ASSERT_EQ(words.size(), 3);
    EXPECT_EQ(words[0].lineIndex, 0);
    EXPECT_EQ(words[1].lineIndex, 1);
    EXPECT_EQ(words[2].lineIndex, 1);
}

TEST(WordsFromPiecesTest, StrayBytesAreRemoved) {
    // Half a character with nothing to complete it, inside a word
    const std::vector<Token> words = wordsFromPieces({piece(" caf\xC3", 0, 100), piece(" ok", 100, 200)});

    EXPECT_EQ(textsOf(words), (std::vector<std::string>{"caf", "ok"}));
}

TEST(ValidUtf8Test, KeepsValidTextAndDropsStrayBytes) {
    EXPECT_EQ(validUtf8("caf\xC3\xA9 \xE2\x99\xAA \xF0\x9F\x8E\xB5"), "caf\xC3\xA9 \xE2\x99\xAA \xF0\x9F\x8E\xB5");
    EXPECT_EQ(validUtf8("a\x99" "b"), "ab");
    EXPECT_EQ(validUtf8("\xE2\x99"), "");
    EXPECT_EQ(validUtf8("\xC0\xAF"), "");          // Overlong
    EXPECT_EQ(validUtf8("\xED\xA0\x80" "x"), "x");  // UTF-16 surrogate
    EXPECT_EQ(validUtf8(""), "");
}
