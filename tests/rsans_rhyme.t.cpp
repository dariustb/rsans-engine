#include <rsans_rhyme.h>

#include <rsans_data.h>

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <vector>

namespace {

// Helper to create a temporary dictionary file for testing
class TempDictFile {
public:
    TempDictFile(const std::string& content) : path_("/tmp/test_cmudict_" + std::to_string(reinterpret_cast<uintptr_t>(this)) + ".dict") {
        std::ofstream file(path_);
        file << content;
    }

    ~TempDictFile() {
        std::remove(path_.c_str());
    }

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

// Helper function to create a basic ProjectData JSON for testing
std::string createTestProjectJson(const std::vector<Token>& tokens, const std::string& cmudictPath = "") {
    std::ostringstream json;
    json << R"({
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": { "path": "test.wav", "length": 10.5 },
        "video": { "width": 1920, "height": 1080, "background": "#000000" },
        "layout": { "fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60 },
        "model": { "path": "models/base.bin" },)";

    if (!cmudictPath.empty()) {
        json << "\"cmudict\": \"" << cmudictPath << "\",";
    } else {
        json << "\"cmudict\": null,";
    }

    json << R"("colorSwatch": [],
        "tokens": [)";

    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"id\":" << tokens[i].id
             << ",\"text\":\"" << tokens[i].text << "\""
             << ",\"startMs\":" << tokens[i].startMs
             << ",\"endMs\":" << tokens[i].endMs
             << ",\"lineIndex\":" << tokens[i].lineIndex
             << ",\"rhymeIndex\":";
        if (tokens[i].rhymeIndex.has_value()) {
            json << tokens[i].rhymeIndex.value();
        } else {
            json << "null";
        }
        json << "}";
    }

    json << "]}";
    return json.str();
}

} // namespace

TEST(CMUDictTest, IsVowelReturnsTrueGivenPhonemeWithStressMarkerZero) {
    // Given: A phoneme ending with stress marker 0
    const std::string phoneme = "AH0";

    // When
    const bool result = CMUDict::isVowel(phoneme);

    // Then
    EXPECT_TRUE(result);
}

TEST(CMUDictTest, IsVowelReturnsTrueGivenPhonemeWithStressMarkerOne) {
    // Given: A phoneme ending with primary stress marker 1
    const std::string phoneme = "AE1";

    // When
    const bool result = CMUDict::isVowel(phoneme);

    // Then
    EXPECT_TRUE(result);
}

TEST(CMUDictTest, IsVowelReturnsTrueGivenPhonemeWithStressMarkerTwo) {
    // Given: A phoneme ending with secondary stress marker 2
    const std::string phoneme = "IY2";

    // When
    const bool result = CMUDict::isVowel(phoneme);

    // Then
    EXPECT_TRUE(result);
}

TEST(CMUDictTest, IsVowelReturnsFalseGivenConsonantPhoneme) {
    // Given: A consonant phoneme (no stress marker)
    const std::string phoneme = "K";

    // When
    const bool result = CMUDict::isVowel(phoneme);

    // Then
    EXPECT_FALSE(result);
}

TEST(CMUDictTest, IsVowelReturnsFalseGivenEmptyString) {
    // Given: An empty phoneme string
    const std::string phoneme = "";

    // When
    const bool result = CMUDict::isVowel(phoneme);

    // Then
    EXPECT_FALSE(result);
}

TEST(CMUDictTest, IsVowelReturnsFalseGivenMultiCharConsonant) {
    // Given: A multi-character consonant phoneme
    const std::string phoneme = "TH";

    // When
    const bool result = CMUDict::isVowel(phoneme);

    // Then
    EXPECT_FALSE(result);
}

TEST(CMUDictTest, ExtractRhymeTailReturnsFromStressedVowelGivenSimpleWord) {
    // Given: Phonemes for "cat" = K AE1 T
    const std::vector<std::string> phonemes = {"K", "AE1", "T"};

    // When
    const std::vector<std::string> tail = CMUDict::extractRhymeTail(phonemes);

    // Then: Should return from AE1 to end
    ASSERT_EQ(tail.size(), 2);
    EXPECT_EQ(tail[0], "AE1");
    EXPECT_EQ(tail[1], "T");
}

TEST(CMUDictTest, ExtractRhymeTailReturnsLastStressedVowelGivenMultiSyllable) {
    // Given: Phonemes for "about" = AH0 B AW1 T
    const std::vector<std::string> phonemes = {"AH0", "B", "AW1", "T"};

    // When
    const std::vector<std::string> tail = CMUDict::extractRhymeTail(phonemes);

    // Then: Should return from AW1 (primary stress) to end
    ASSERT_EQ(tail.size(), 2);
    EXPECT_EQ(tail[0], "AW1");
    EXPECT_EQ(tail[1], "T");
}

TEST(CMUDictTest, ExtractRhymeTailFallsBackToSecondaryStressGivenNoPrimaryStress) {
    // Given: Phonemes with only secondary stress (stress marker 2)
    const std::vector<std::string> phonemes = {"K", "AE2", "T"};

    // When
    const std::vector<std::string> tail = CMUDict::extractRhymeTail(phonemes);

    // Then
    ASSERT_EQ(tail.size(), 2);
    EXPECT_EQ(tail[0], "AE2");
    EXPECT_EQ(tail[1], "T");
}

TEST(CMUDictTest, ExtractRhymeTailFallsBackToAnyVowelGivenOnlyUnstressedVowels) {
    // Given: Phonemes with only unstressed vowel (stress marker 0)
    const std::vector<std::string> phonemes = {"DH", "AH0"};

    // When
    const std::vector<std::string> tail = CMUDict::extractRhymeTail(phonemes);

    // Then
    ASSERT_EQ(tail.size(), 1);
    EXPECT_EQ(tail[0], "AH0");
}

TEST(CMUDictTest, ExtractRhymeTailReturnsEmptyGivenNoVowels) {
    // Given: Phonemes with no vowels (consonants only)
    const std::vector<std::string> phonemes = {"S", "T", "R"};

    // When
    const std::vector<std::string> tail = CMUDict::extractRhymeTail(phonemes);

    // Then
    EXPECT_TRUE(tail.empty());
}

TEST(CMUDictTest, ExtractRhymeTailReturnsEmptyGivenEmptyInput) {
    // Given: Empty phoneme vector
    const std::vector<std::string> phonemes = {};

    // When
    const std::vector<std::string> tail = CMUDict::extractRhymeTail(phonemes);

    // Then
    EXPECT_TRUE(tail.empty());
}

TEST(CMUDictTest, ExtractRhymeTailReturnsEntireWordGivenVowelAtStart) {
    // Given: Phonemes for "at" = AE1 T
    const std::vector<std::string> phonemes = {"AE1", "T"};

    // When
    const std::vector<std::string> tail = CMUDict::extractRhymeTail(phonemes);

    // Then
    ASSERT_EQ(tail.size(), 2);
    EXPECT_EQ(tail[0], "AE1");
    EXPECT_EQ(tail[1], "T");
}

TEST(CMUDictTest, ExtractVowelsReturnsVowelsWithoutStressGivenMixedPhonemes) {
    // Given: Phonemes with mixed vowels and consonants
    const std::vector<std::string> phonemes = {"AE1", "T"};

    // When
    const std::vector<std::string> vowels = CMUDict::extractVowels(phonemes);

    // Then: Should return vowel with stress marker stripped
    ASSERT_EQ(vowels.size(), 1);
    EXPECT_EQ(vowels[0], "AE");
}

TEST(CMUDictTest, ExtractVowelsReturnsMultipleVowelsGivenMultiSyllable) {
    // Given: Phonemes with multiple vowels
    const std::vector<std::string> phonemes = {"AH0", "B", "AW1", "T"};

    // When
    const std::vector<std::string> vowels = CMUDict::extractVowels(phonemes);

    // Then
    ASSERT_EQ(vowels.size(), 2);
    EXPECT_EQ(vowels[0], "AH");
    EXPECT_EQ(vowels[1], "AW");
}

TEST(CMUDictTest, ExtractVowelsReturnsEmptyGivenOnlyConsonants) {
    // Given: Only consonant phonemes
    const std::vector<std::string> phonemes = {"S", "T", "R"};

    // When
    const std::vector<std::string> vowels = CMUDict::extractVowels(phonemes);

    // Then
    EXPECT_TRUE(vowels.empty());
}

TEST(CMUDictTest, ExtractConsonantsReturnsConsonantsGivenMixedPhonemes) {
    // Given: Phonemes with mixed vowels and consonants
    const std::vector<std::string> phonemes = {"K", "AE1", "T"};

    // When
    const std::vector<std::string> consonants = CMUDict::extractConsonants(phonemes);

    // Then
    ASSERT_EQ(consonants.size(), 2);
    EXPECT_EQ(consonants[0], "K");
    EXPECT_EQ(consonants[1], "T");
}

TEST(CMUDictTest, ExtractConsonantsReturnsEmptyGivenOnlyVowels) {
    // Given: Only vowel phonemes
    const std::vector<std::string> phonemes = {"AH0", "IY1"};

    // When
    const std::vector<std::string> consonants = CMUDict::extractConsonants(phonemes);

    // Then
    EXPECT_TRUE(consonants.empty());
}

TEST(CMUDictTest, GetPhonemesReturnsCorrectPhonemesGivenValidWord) {
    // Given: A dictionary with a single entry
    TempDictFile dictFile("cat K AE1 T\n");
    CMUDict dict(dictFile.path());

    // When
    const std::vector<std::string> phonemes = dict.getPhonemes("cat");

    // Then
    ASSERT_EQ(phonemes.size(), 3);
    EXPECT_EQ(phonemes[0], "K");
    EXPECT_EQ(phonemes[1], "AE1");
    EXPECT_EQ(phonemes[2], "T");
}

TEST(CMUDictTest, GetPhonemesReturnsEmptyGivenUnknownWord) {
    // Given: A dictionary without the queried word
    TempDictFile dictFile("cat K AE1 T\n");
    CMUDict dict(dictFile.path());

    // When
    const std::vector<std::string> phonemes = dict.getPhonemes("dog");

    // Then
    EXPECT_TRUE(phonemes.empty());
}

TEST(CMUDictTest, GetPhonemesIsCaseInsensitiveGivenUppercaseQuery) {
    // Given: A dictionary with lowercase entry
    TempDictFile dictFile("hello HH AH0 L OW1\n");
    CMUDict dict(dictFile.path());

    // When
    const std::vector<std::string> phonemes = dict.getPhonemes("HELLO");

    // Then
    ASSERT_EQ(phonemes.size(), 4);
    EXPECT_EQ(phonemes[0], "HH");
}

TEST(CMUDictTest, GetPhonemesHandlesAlternatePronunciationsGivenWordWithVariants) {
    // Given: A dictionary with alternate pronunciations
    TempDictFile dictFile("read R IY1 D\nread(2) R EH1 D\n");
    CMUDict dict(dictFile.path());

    // When: Query the word
    const std::vector<std::string> phonemes = dict.getPhonemes("read");

    // Then: Should return first pronunciation
    ASSERT_EQ(phonemes.size(), 3);
    EXPECT_EQ(phonemes[0], "R");
    EXPECT_EQ(phonemes[1], "IY1");
    EXPECT_EQ(phonemes[2], "D");
}

TEST(CMUDictTest, GetPhonemesSkipsCommentsGivenDictWithComments) {
    // Given: A dictionary with inline comments
    TempDictFile dictFile("aalborg AO1 L B AO0 R G # place, danish\n");
    CMUDict dict(dictFile.path());

    // When
    const std::vector<std::string> phonemes = dict.getPhonemes("aalborg");

    // Then: Should not include comment as phoneme
    ASSERT_EQ(phonemes.size(), 6);
    EXPECT_EQ(phonemes[5], "G");
}

TEST(CMUDictTest, GetPhonemesHandlesApostropheWordsGivenContractions) {
    // Given: A dictionary with apostrophe words
    TempDictFile dictFile("'bout B AW1 T\n");
    CMUDict dict(dictFile.path());

    // When
    const std::vector<std::string> phonemes = dict.getPhonemes("'bout");

    // Then
    ASSERT_EQ(phonemes.size(), 3);
    EXPECT_EQ(phonemes[0], "B");
}

TEST(CMUDictTest, ConstructorHandlesNonExistentFileGivenInvalidPath) {
    // Given: A path to a non-existent file
    CMUDict dict("/nonexistent/path/to/dict.dict");

    // When
    const std::vector<std::string> phonemes = dict.getPhonemes("any");

    // Then: Should return empty without crashing
    EXPECT_TRUE(phonemes.empty());
}

TEST(RhymeGrouperTest, FindReturnsElementItselfGivenNewElement) {
    // Given: A fresh grouper
    RhymeGrouper grouper;

    // When
    const int root = grouper.find(5);

    // Then: Element is its own root initially
    EXPECT_EQ(root, 5);
}

TEST(RhymeGrouperTest, FindReturnsSameRootGivenUnitedElements) {
    // Given: Two elements united together
    RhymeGrouper grouper;
    grouper.unite(1, 2);

    // When
    const int root1 = grouper.find(1);
    const int root2 = grouper.find(2);

    // Then: Both should have the same root
    EXPECT_EQ(root1, root2);
}

TEST(RhymeGrouperTest, UniteCreatesTransitiveGroupsGivenChainedUnions) {
    // Given: A chain of unions: 1-2, 2-3
    RhymeGrouper grouper;
    grouper.unite(1, 2);
    grouper.unite(2, 3);

    // When
    const int root1 = grouper.find(1);
    const int root3 = grouper.find(3);

    // Then: 1 and 3 should be in same group transitively
    EXPECT_EQ(root1, root3);
}

TEST(RhymeGrouperTest, FindReturnsDifferentRootsGivenDisjointElements) {
    // Given: Elements that haven't been united
    RhymeGrouper grouper;
    grouper.find(1);
    grouper.find(2);

    // When
    const int root1 = grouper.find(1);
    const int root2 = grouper.find(2);

    // Then: Should have different roots
    EXPECT_NE(root1, root2);
}

TEST(RhymeGrouperTest, UniteMergesGroupsGivenTwoSeparateGroups) {
    // Given: Two separate groups {1,2} and {3,4}
    RhymeGrouper grouper;
    grouper.unite(1, 2);
    grouper.unite(3, 4);

    // When: Unite the two groups
    grouper.unite(2, 3);

    // Then: All four elements should be in the same group
    const int root1 = grouper.find(1);
    const int root4 = grouper.find(4);
    EXPECT_EQ(root1, root4);
}

TEST(DetectRhymesTest, DetectRhymesAssignsGroupGivenPerfectRhymes) {
    // Given: Tokens with perfect rhymes (cat/hat)
    TempDictFile dictFile("cat K AE1 T\nhat HH AE1 T\n");
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, std::nullopt},
        {2, "hat", 500, 1000, 1, std::nullopt}
    };
    const ProjectData input(createTestProjectJson(tokens, dictFile.path()));

    // When
    const ProjectData result = detectRhymes(input);

    // Then: Both tokens should have the same rhyme group
    ASSERT_TRUE(result.tokens[0].rhymeIndex.has_value());
    ASSERT_TRUE(result.tokens[1].rhymeIndex.has_value());
    EXPECT_EQ(result.tokens[0].rhymeIndex.value(), result.tokens[1].rhymeIndex.value());
}

TEST(DetectRhymesTest, DetectRhymesAssignsNoGroupGivenNonRhymingWords) {
    // Given: Tokens that don't rhyme
    TempDictFile dictFile("cat K AE1 T\ndog D AO1 G\n");
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, std::nullopt},
        {2, "dog", 500, 1000, 1, std::nullopt}
    };
    const ProjectData input(createTestProjectJson(tokens, dictFile.path()));

    // When
    const ProjectData result = detectRhymes(input);

    // Then: Neither token should have a rhyme group (singletons are filtered)
    EXPECT_FALSE(result.tokens[0].rhymeIndex.has_value());
    EXPECT_FALSE(result.tokens[1].rhymeIndex.has_value());
}

TEST(DetectRhymesTest, DetectRhymesMergesGroupsGivenAssonance) {
    // Given: Words with same vowel sound but different consonants (assonance)
    // "lake" (L EY1 K) and "fade" (F EY1 D) share EY vowel
    TempDictFile dictFile("lake L EY1 K\nfade F EY1 D\n");
    std::vector<Token> tokens = {
        {1, "lake", 0, 500, 0, std::nullopt},
        {2, "fade", 500, 1000, 1, std::nullopt}
    };
    const ProjectData input(createTestProjectJson(tokens, dictFile.path()));

    // When
    const ProjectData result = detectRhymes(input);

    // Then: Both should be in same group via vowel matching
    ASSERT_TRUE(result.tokens[0].rhymeIndex.has_value());
    ASSERT_TRUE(result.tokens[1].rhymeIndex.has_value());
    EXPECT_EQ(result.tokens[0].rhymeIndex.value(), result.tokens[1].rhymeIndex.value());
}

TEST(DetectRhymesTest, DetectRhymesDoesNotGroupGivenOnlyConsonanceMatch) {
    // Given: Words with same ending consonants but different vowels
    // "cat" (K AE1 T) and "cut" (K AH1 T) share consonants but not vowels
    TempDictFile dictFile("cat K AE1 T\ncut K AH1 T\n");
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, std::nullopt},
        {2, "cut", 500, 1000, 1, std::nullopt}
    };
    const ProjectData input(createTestProjectJson(tokens, dictFile.path()));

    // When
    const ProjectData result = detectRhymes(input);

    // Then: Should NOT be grouped (consonance alone is not enough)
    EXPECT_FALSE(result.tokens[0].rhymeIndex.has_value());
    EXPECT_FALSE(result.tokens[1].rhymeIndex.has_value());
}

TEST(DetectRhymesTest, DetectRhymesHandlesUnknownWordsGivenWordsNotInDict) {
    // Given: Token with word not in dictionary
    TempDictFile dictFile("cat K AE1 T\n");
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, std::nullopt},
        {2, "xyzzy", 500, 1000, 1, std::nullopt}
    };
    const ProjectData input(createTestProjectJson(tokens, dictFile.path()));

    // When
    const ProjectData result = detectRhymes(input);

    // Then: Unknown word should have no rhyme group, known word is singleton so also none
    EXPECT_FALSE(result.tokens[0].rhymeIndex.has_value());
    EXPECT_FALSE(result.tokens[1].rhymeIndex.has_value());
}

TEST(DetectRhymesTest, DetectRhymesReturnsUnchangedDataGivenEmptyTokens) {
    // Given: Project with no tokens
    TempDictFile dictFile("cat K AE1 T\n");
    std::vector<Token> tokens = {};
    const ProjectData input(createTestProjectJson(tokens, dictFile.path()));

    // When
    const ProjectData result = detectRhymes(input);

    // Then: Should return data with empty tokens
    EXPECT_TRUE(result.tokens.empty());
}

TEST(DetectRhymesTest, DetectRhymesReturnsUnchangedDataGivenSingleToken) {
    // Given: Project with single token
    TempDictFile dictFile("cat K AE1 T\n");
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, std::nullopt}
    };
    const ProjectData input(createTestProjectJson(tokens, dictFile.path()));

    // When
    const ProjectData result = detectRhymes(input);

    // Then: Single token can't form a rhyme group
    EXPECT_FALSE(result.tokens[0].rhymeIndex.has_value());
}

TEST(DetectRhymesTest, DetectRhymesNormalizesWordGivenPunctuation) {
    // Given: Token with punctuation
    TempDictFile dictFile("cat K AE1 T\nhat HH AE1 T\n");
    std::vector<Token> tokens = {
        {1, "cat,", 0, 500, 0, std::nullopt},
        {2, "hat!", 500, 1000, 1, std::nullopt}
    };
    const ProjectData input(createTestProjectJson(tokens, dictFile.path()));

    // When
    const ProjectData result = detectRhymes(input);

    // Then: Should still detect rhyme after stripping punctuation
    ASSERT_TRUE(result.tokens[0].rhymeIndex.has_value());
    ASSERT_TRUE(result.tokens[1].rhymeIndex.has_value());
    EXPECT_EQ(result.tokens[0].rhymeIndex.value(), result.tokens[1].rhymeIndex.value());
}

TEST(DetectRhymesTest, DetectRhymesAssignsMultipleGroupsGivenDistinctRhymeSets) {
    // Given: Two sets of rhyming words that don't rhyme with each other
    TempDictFile dictFile("cat K AE1 T\nhat HH AE1 T\ndog D AO1 G\nlog L AO1 G\n");
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, std::nullopt},
        {2, "dog", 500, 1000, 0, std::nullopt},
        {3, "hat", 1000, 1500, 1, std::nullopt},
        {4, "log", 1500, 2000, 1, std::nullopt}
    };
    const ProjectData input(createTestProjectJson(tokens, dictFile.path()));

    // When
    const ProjectData result = detectRhymes(input);

    // Then: cat/hat should be one group, dog/log another
    ASSERT_TRUE(result.tokens[0].rhymeIndex.has_value()); // cat
    ASSERT_TRUE(result.tokens[1].rhymeIndex.has_value()); // dog
    ASSERT_TRUE(result.tokens[2].rhymeIndex.has_value()); // hat
    ASSERT_TRUE(result.tokens[3].rhymeIndex.has_value()); // log

    EXPECT_EQ(result.tokens[0].rhymeIndex.value(), result.tokens[2].rhymeIndex.value()); // cat = hat
    EXPECT_EQ(result.tokens[1].rhymeIndex.value(), result.tokens[3].rhymeIndex.value()); // dog = log
    EXPECT_NE(result.tokens[0].rhymeIndex.value(), result.tokens[1].rhymeIndex.value()); // cat != dog
}

TEST(DetectRhymesTest, DetectRhymesHandlesThreeWayRhymeGivenThreeRhymingWords) {
    // Given: Three words that all rhyme
    TempDictFile dictFile("cat K AE1 T\nhat HH AE1 T\nmat M AE1 T\n");
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, std::nullopt},
        {2, "hat", 500, 1000, 0, std::nullopt},
        {3, "mat", 1000, 1500, 1, std::nullopt}
    };
    const ProjectData input(createTestProjectJson(tokens, dictFile.path()));

    // When
    const ProjectData result = detectRhymes(input);

    // Then: All three should be in the same group
    ASSERT_TRUE(result.tokens[0].rhymeIndex.has_value());
    ASSERT_TRUE(result.tokens[1].rhymeIndex.has_value());
    ASSERT_TRUE(result.tokens[2].rhymeIndex.has_value());
    EXPECT_EQ(result.tokens[0].rhymeIndex.value(), result.tokens[1].rhymeIndex.value());
    EXPECT_EQ(result.tokens[1].rhymeIndex.value(), result.tokens[2].rhymeIndex.value());
}

TEST(DetectRhymesTest, DetectRhymesAssignsNonNegativeIndexGivenRhymingTokens) {
    // Given: Two rhyming words
    TempDictFile dictFile("cat K AE1 T\nhat HH AE1 T\n");
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, std::nullopt},
        {2, "hat", 500, 1000, 1, std::nullopt}
    };
    const ProjectData input(createTestProjectJson(tokens, dictFile.path()));

    // When
    const ProjectData result = detectRhymes(input);

    // Then: rhymeIndex should be a non-negative integer
    ASSERT_TRUE(result.tokens[0].rhymeIndex.has_value());
    EXPECT_GE(result.tokens[0].rhymeIndex.value(), 0);
}
