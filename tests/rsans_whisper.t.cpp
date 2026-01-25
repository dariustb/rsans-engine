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
