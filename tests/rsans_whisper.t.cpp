#include <rsans_whisper.h>

#include <rsans_data.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

TEST(WhisperTest, ExtractTokensFromAudioReturnsEmptyVectorGivenNonExistentAudioFile) {
    // Given
    const std::string audioPath = "/nonexistent/path/to/audio.wav";
    const std::string modelPath = "/nonexistent/path/to/model.bin";

    // When
    const std::vector<Token> tokens = extractTokensFromAudio(audioPath, modelPath);

    // Then
    EXPECT_TRUE(tokens.empty());
}

TEST(WhisperTest, ExtractTokensFromAudioReturnsEmptyVectorGivenEmptyPaths) {
    // Given
    const std::string audioPath = "";
    const std::string modelPath = "";

    // When
    const std::vector<Token> tokens = extractTokensFromAudio(audioPath, modelPath);

    // Then
    EXPECT_TRUE(tokens.empty());
}
