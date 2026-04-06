#ifndef INCLUDED_RSANS_LYRIC_TOKENIZER
#define INCLUDED_RSANS_LYRIC_TOKENIZER

#include <rsans_data.h>

#include <string>
#include <vector>

struct LyricToken {
    std::string text;       // Original text, preserved for display.
    std::string normalized; // Lowercase, punctuation-stripped; empty for punctuation-only tokens.

    int lineIndex;
    int tokenInLine;
    int globalIndex;

    bool alignable() const;
};

struct LyricLine {
    std::string text;
    std::vector<LyricToken> tokens;
    int lineIndex;

    bool blank() const;
};

struct LyricSheet {
    std::vector<LyricLine> lines;

    // Alignable tokens in reading order. Feed to the alignment stage;
    // write timestamps back via globalIndex.
    std::vector<const LyricToken*> alignableTokens() const;
};

// Throws std::runtime_error if the file cannot be opened or contains no words.
LyricSheet parseLyrics(const std::string& path);

// Match lyric tokens to Whisper output by normalized text.
// Unmatched tokens receive interpolated timestamps from their neighbours.
// Returns a flat vector<Token> in reading order, suitable for ProjectData.
std::vector<Token> alignLyricsToWhisper(
    const LyricSheet& sheet,
    const std::vector<Token>& whisperTokens
);

// Lowercase, strip surrounding punctuation, preserve internal apostrophes.
// Returns empty for punctuation-only input.
std::string normalizeLyricWord(const std::string& word);

#endif
