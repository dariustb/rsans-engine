#include <rsans_lyric_tokenizer.h>

#include <gtest/gtest.h>

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class TempLyricsFile {
public:
    TempLyricsFile(const std::string& content)
        : path_("/tmp/test_lyrics_" + std::to_string(reinterpret_cast<uintptr_t>(this)) + ".txt")
    {
        std::ofstream file(path_);
        file << content;
    }

    ~TempLyricsFile() {
        std::remove(path_.c_str());
    }

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

} // namespace

// normalizeLyricWord

TEST(NormalizeLyricWordTest, ReturnsEmptyGivenEmptyString) {
    // Given
    const std::string word = "";

    // When
    const std::string result = normalizeLyricWord(word);

    // Then
    EXPECT_TRUE(result.empty());
}

TEST(NormalizeLyricWordTest, ReturnsEmptyGivenPunctuationOnly) {
    // Given
    const std::string word = "...";

    // When
    const std::string result = normalizeLyricWord(word);

    // Then
    EXPECT_TRUE(result.empty());
}

TEST(NormalizeLyricWordTest, ReturnsLowercaseGivenUppercaseWord) {
    // Given
    const std::string word = "HELLO";

    // When
    const std::string result = normalizeLyricWord(word);

    // Then
    EXPECT_EQ(result, "hello");
}

TEST(NormalizeLyricWordTest, StripsLeadingPunctuationGivenPrefixedWord) {
    // Given
    const std::string word = "\"hello";

    // When
    const std::string result = normalizeLyricWord(word);

    // Then
    EXPECT_EQ(result, "hello");
}

TEST(NormalizeLyricWordTest, StripsTrailingPunctuationGivenSuffixedWord) {
    // Given
    const std::string word = "hello,";

    // When
    const std::string result = normalizeLyricWord(word);

    // Then
    EXPECT_EQ(result, "hello");
}

TEST(NormalizeLyricWordTest, StripsLeadingAndTrailingPunctuationGivenBothSides) {
    // Given
    const std::string word = "\"hello!\"";

    // When
    const std::string result = normalizeLyricWord(word);

    // Then
    EXPECT_EQ(result, "hello");
}

TEST(NormalizeLyricWordTest, PreservesInternalApostropheGivenContraction) {
    // Given
    const std::string word = "don't";

    // When
    const std::string result = normalizeLyricWord(word);

    // Then
    EXPECT_EQ(result, "don't");
}

TEST(NormalizeLyricWordTest, PreservesInternalApostropheGivenPossessive) {
    // Given
    const std::string word = "it's";

    // When
    const std::string result = normalizeLyricWord(word);

    // Then
    EXPECT_EQ(result, "it's");
}

TEST(NormalizeLyricWordTest, DropsLeadingApostropheGivenApostrophePrefix) {
    // Given: Leading apostrophe falls outside the first-to-last alnum span
    const std::string word = "'bout";

    // When
    const std::string result = normalizeLyricWord(word);

    // Then
    EXPECT_EQ(result, "bout");
}

TEST(NormalizeLyricWordTest, DropsInternalHyphenGivenHyphenatedWord) {
    // Given: Only apostrophes are kept within the span; hyphens are dropped
    const std::string word = "well-known";

    // When
    const std::string result = normalizeLyricWord(word);

    // Then
    EXPECT_EQ(result, "wellknown");
}

// LyricToken

TEST(LyricTokenTest, AlignableReturnsTrueGivenNonEmptyNormalized) {
    // Given
    LyricToken tok;
    tok.text = "hello";
    tok.normalized = "hello";
    tok.lineIndex = 0;
    tok.tokenInLine = 0;
    tok.globalIndex = 0;

    // When
    const bool result = tok.alignable();

    // Then
    EXPECT_TRUE(result);
}

TEST(LyricTokenTest, AlignableReturnsFalseGivenEmptyNormalized) {
    // Given: Punctuation-only token has empty normalized
    LyricToken tok;
    tok.text = "...";
    tok.normalized = "";
    tok.lineIndex = 0;
    tok.tokenInLine = 0;
    tok.globalIndex = 0;

    // When
    const bool result = tok.alignable();

    // Then
    EXPECT_FALSE(result);
}

// LyricLine

TEST(LyricLineTest, BlankReturnsTrueGivenNoTokens) {
    // Given
    LyricLine line;
    line.text = "";
    line.lineIndex = 0;

    // When
    const bool result = line.blank();

    // Then
    EXPECT_TRUE(result);
}

TEST(LyricLineTest, BlankReturnsFalseGivenTokens) {
    // Given
    LyricLine line;
    line.text = "hello";
    line.lineIndex = 0;

    LyricToken tok;
    tok.text = "hello";
    tok.normalized = "hello";
    tok.lineIndex = 0;
    tok.tokenInLine = 0;
    tok.globalIndex = 0;
    line.tokens.push_back(tok);

    // When
    const bool result = line.blank();

    // Then
    EXPECT_FALSE(result);
}

// LyricSheet

TEST(LyricSheetTest, AlignableTokensReturnsEmptyGivenNoLines) {
    // Given
    LyricSheet sheet;

    // When
    const auto result = sheet.alignableTokens();

    // Then
    EXPECT_TRUE(result.empty());
}

TEST(LyricSheetTest, AlignableTokensExcludesNonAlignableTokens) {
    // Given: One word token and one punctuation-only token on the same line
    TempLyricsFile file("hello ...\n");
    const LyricSheet sheet = parseLyrics(file.path());

    // When
    const auto result = sheet.alignableTokens();

    // Then: Only "hello" is returned; "..." is excluded
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0]->text, "hello");
}

TEST(LyricSheetTest, AlignableTokensReturnsInReadingOrder) {
    // Given: Two lines, one word each
    TempLyricsFile file("first\nsecond\n");
    const LyricSheet sheet = parseLyrics(file.path());

    // When
    const auto result = sheet.alignableTokens();

    // Then
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0]->text, "first");
    EXPECT_EQ(result[1]->text, "second");
}

// parseLyrics

TEST(ParseLyricsTest, ThrowsGivenNonExistentFile) {
    // Given
    const std::string path = "/nonexistent/path/lyrics.txt";

    // When / Then
    EXPECT_THROW(parseLyrics(path), std::runtime_error);
}

TEST(ParseLyricsTest, ThrowsGivenEmptyFile) {
    // Given
    TempLyricsFile file("");

    // When / Then
    EXPECT_THROW(parseLyrics(file.path()), std::runtime_error);
}

TEST(ParseLyricsTest, ThrowsGivenWhitespaceOnlyFile) {
    // Given
    TempLyricsFile file("   \n\n   \n");

    // When / Then
    EXPECT_THROW(parseLyrics(file.path()), std::runtime_error);
}

TEST(ParseLyricsTest, ReturnsCorrectLineCountGivenMultipleLines) {
    // Given
    TempLyricsFile file("hello world\ngoodbye world\n");

    // When
    const LyricSheet sheet = parseLyrics(file.path());

    // Then
    EXPECT_EQ(sheet.lines.size(), 2u);
}

TEST(ParseLyricsTest, PreservesBlankLinesGivenInterspersedBlanks) {
    // Given
    TempLyricsFile file("hello\n\nworld\n");

    // When
    const LyricSheet sheet = parseLyrics(file.path());

    // Then: Three lines total — content, blank, content
    ASSERT_EQ(sheet.lines.size(), 3u);
    EXPECT_TRUE(sheet.lines[1].blank());
}

TEST(ParseLyricsTest, SetsLineTextGivenOriginalContent) {
    // Given
    TempLyricsFile file("hello world\n");

    // When
    const LyricSheet sheet = parseLyrics(file.path());

    // Then: Original line text preserved exactly
    ASSERT_EQ(sheet.lines.size(), 1u);
    EXPECT_EQ(sheet.lines[0].text, "hello world");
}

TEST(ParseLyricsTest, TokenizesLineIntoWordsGivenSpaceSeparatedWords) {
    // Given
    TempLyricsFile file("one two three\n");

    // When
    const LyricSheet sheet = parseLyrics(file.path());

    // Then
    ASSERT_EQ(sheet.lines[0].tokens.size(), 3u);
    EXPECT_EQ(sheet.lines[0].tokens[0].text, "one");
    EXPECT_EQ(sheet.lines[0].tokens[1].text, "two");
    EXPECT_EQ(sheet.lines[0].tokens[2].text, "three");
}

TEST(ParseLyricsTest, SetsCorrectLineIndexGivenMultipleLines) {
    // Given
    TempLyricsFile file("first line\nsecond line\n");

    // When
    const LyricSheet sheet = parseLyrics(file.path());

    // Then: Both LyricLine and its tokens carry the correct lineIndex
    EXPECT_EQ(sheet.lines[0].lineIndex, 0);
    EXPECT_EQ(sheet.lines[1].lineIndex, 1);
    EXPECT_EQ(sheet.lines[0].tokens[0].lineIndex, 0);
    EXPECT_EQ(sheet.lines[1].tokens[0].lineIndex, 1);
}

TEST(ParseLyricsTest, SetsCorrectTokenInLineIndexGivenMultipleTokensPerLine) {
    // Given
    TempLyricsFile file("alpha beta gamma\n");

    // When
    const LyricSheet sheet = parseLyrics(file.path());

    // Then
    EXPECT_EQ(sheet.lines[0].tokens[0].tokenInLine, 0);
    EXPECT_EQ(sheet.lines[0].tokens[1].tokenInLine, 1);
    EXPECT_EQ(sheet.lines[0].tokens[2].tokenInLine, 2);
}

TEST(ParseLyricsTest, SetsCorrectGlobalIndexGivenMultipleLines) {
    // Given: Two lines with two tokens each
    TempLyricsFile file("a b\nc d\n");

    // When
    const LyricSheet sheet = parseLyrics(file.path());

    // Then: Global index increments continuously across lines
    EXPECT_EQ(sheet.lines[0].tokens[0].globalIndex, 0);
    EXPECT_EQ(sheet.lines[0].tokens[1].globalIndex, 1);
    EXPECT_EQ(sheet.lines[1].tokens[0].globalIndex, 2);
    EXPECT_EQ(sheet.lines[1].tokens[1].globalIndex, 3);
}

TEST(ParseLyricsTest, NormalizesTokenGivenPunctuatedWord) {
    // Given
    TempLyricsFile file("hello,\n");

    // When
    const LyricSheet sheet = parseLyrics(file.path());

    // Then: Original text preserved; normalized has punctuation stripped
    ASSERT_EQ(sheet.lines[0].tokens.size(), 1u);
    EXPECT_EQ(sheet.lines[0].tokens[0].text,       "hello,");
    EXPECT_EQ(sheet.lines[0].tokens[0].normalized, "hello");
}

TEST(ParseLyricsTest, MarksWordTokenAlignableGivenNormalWord) {
    // Given
    TempLyricsFile file("hello\n");

    // When
    const LyricSheet sheet = parseLyrics(file.path());

    // Then
    ASSERT_EQ(sheet.lines[0].tokens.size(), 1u);
    EXPECT_TRUE(sheet.lines[0].tokens[0].alignable());
}

TEST(ParseLyricsTest, MarksStandalonePunctuationNonAlignableGivenPunctuationOnlyToken) {
    // Given
    TempLyricsFile file("hello ...\n");

    // When
    const LyricSheet sheet = parseLyrics(file.path());

    // Then
    ASSERT_EQ(sheet.lines[0].tokens.size(), 2u);
    EXPECT_TRUE(sheet.lines[0].tokens[0].alignable());   // "hello"
    EXPECT_FALSE(sheet.lines[0].tokens[1].alignable());  // "..."
}

// alignLyricsToWhisper

TEST(AlignLyricsToWhisperTest, AssignsZeroTimestampsGivenEmptyWhisperOutput) {
    // Given
    TempLyricsFile file("hello world\n");
    const LyricSheet sheet = parseLyrics(file.path());
    const std::vector<Token> whisperTokens = {};

    // When
    const std::vector<Token> result = alignLyricsToWhisper(sheet, whisperTokens);

    // Then: Tokens present but all timestamps fall back to 0
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].startMs, 0);
    EXPECT_EQ(result[0].endMs,   0);
    EXPECT_EQ(result[1].startMs, 0);
    EXPECT_EQ(result[1].endMs,   0);
}

TEST(AlignLyricsToWhisperTest, AssignsTimestampsGivenExactTextMatch) {
    // Given
    TempLyricsFile file("hello world\n");
    const LyricSheet sheet = parseLyrics(file.path());
    const std::vector<Token> whisperTokens = {
        {0, "hello", 0,   500,  0, std::nullopt},
        {1, "world", 500, 1000, 0, std::nullopt}
    };

    // When
    const std::vector<Token> result = alignLyricsToWhisper(sheet, whisperTokens);

    // Then
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].startMs, 0);
    EXPECT_EQ(result[0].endMs,   500);
    EXPECT_EQ(result[1].startMs, 500);
    EXPECT_EQ(result[1].endMs,   1000);
}

TEST(AlignLyricsToWhisperTest, AssignsTimestampsGivenNormalizedMatch) {
    // Given: Lyric token has trailing punctuation; Whisper token does not
    TempLyricsFile file("hello,\n");
    const LyricSheet sheet = parseLyrics(file.path());
    const std::vector<Token> whisperTokens = {
        {0, "hello", 100, 400, 0, std::nullopt}
    };

    // When
    const std::vector<Token> result = alignLyricsToWhisper(sheet, whisperTokens);

    // Then: Normalized forms match despite surface difference
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].startMs, 100);
    EXPECT_EQ(result[0].endMs,   400);
}

TEST(AlignLyricsToWhisperTest, SetsTextFromLyricsGivenDifferentWhisperText) {
    // Given: Whisper output is lowercase; lyrics have capitalisation
    TempLyricsFile file("Hello World\n");
    const LyricSheet sheet = parseLyrics(file.path());
    const std::vector<Token> whisperTokens = {
        {0, "hello", 0,   500,  0, std::nullopt},
        {1, "world", 500, 1000, 0, std::nullopt}
    };

    // When
    const std::vector<Token> result = alignLyricsToWhisper(sheet, whisperTokens);

    // Then: Text comes from the lyrics file, not Whisper
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].text, "Hello");
    EXPECT_EQ(result[1].text, "World");
}

TEST(AlignLyricsToWhisperTest, SetsLineIndexFromLyricSheetGivenMultipleLines) {
    // Given
    TempLyricsFile file("hello\nworld\n");
    const LyricSheet sheet = parseLyrics(file.path());
    const std::vector<Token> whisperTokens = {
        {0, "hello", 0,   500,  0, std::nullopt},
        {1, "world", 500, 1000, 0, std::nullopt}
    };

    // When
    const std::vector<Token> result = alignLyricsToWhisper(sheet, whisperTokens);

    // Then: lineIndex reflects the lyric sheet, not Whisper's segment index
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].lineIndex, 0);
    EXPECT_EQ(result[1].lineIndex, 1);
}

TEST(AlignLyricsToWhisperTest, ExcludesPunctuationOnlyTokensFromResult) {
    // Given: Line contains a word and a standalone punctuation token
    TempLyricsFile file("hello ...\n");
    const LyricSheet sheet = parseLyrics(file.path());
    const std::vector<Token> whisperTokens = {
        {0, "hello", 0, 500, 0, std::nullopt}
    };

    // When
    const std::vector<Token> result = alignLyricsToWhisper(sheet, whisperTokens);

    // Then: "..." is excluded; only "hello" is in the result
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].text, "hello");
}

TEST(AlignLyricsToWhisperTest, AssignsSequentialIdsStartingFromZero) {
    // Given
    TempLyricsFile file("one two three\n");
    const LyricSheet sheet = parseLyrics(file.path());
    const std::vector<Token> whisperTokens = {
        {0, "one",   0,   333,  0, std::nullopt},
        {1, "two",   333, 666,  0, std::nullopt},
        {2, "three", 666, 1000, 0, std::nullopt}
    };

    // When
    const std::vector<Token> result = alignLyricsToWhisper(sheet, whisperTokens);

    // Then
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0].id, 0);
    EXPECT_EQ(result[1].id, 1);
    EXPECT_EQ(result[2].id, 2);
}

TEST(AlignLyricsToWhisperTest, InterpolatesTimestampsGivenUnmatchedMiddleToken) {
    // Given: Whisper has first and last but not middle
    TempLyricsFile file("first middle last\n");
    const LyricSheet sheet = parseLyrics(file.path());
    const std::vector<Token> whisperTokens = {
        {0, "first", 0,   100, 0, std::nullopt},
        {1, "last",  200, 300, 0, std::nullopt}
    };

    // When
    const std::vector<Token> result = alignLyricsToWhisper(sheet, whisperTokens);

    // Then: "middle" is interpolated between first's endMs (100) and last's startMs (200)
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[1].startMs, 100);
    EXPECT_EQ(result[1].endMs,   200);
}
