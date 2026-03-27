#include <rsans_lyric_tokenizer.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::vector<std::string> splitOnWhitespace(const std::string& s) {
    std::vector<std::string> parts;
    std::istringstream iss(s);
    std::string word;
    while (iss >> word) {
        parts.push_back(word);
    }
    return parts;
}

bool isPunctuationOnly(const std::string& s) {
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '\'') {
            return false;
        }
    }
    return true;
}

} // namespace

std::string normalizeLyricWord(const std::string& word) {
    if (word.empty()) {
        return {};
    }

    int first = -1;
    int last  = -1;
    for (int i = 0; i < static_cast<int>(word.size()); ++i) {
        if (std::isalnum(static_cast<unsigned char>(word[i]))) {
            if (first < 0) first = i;
            last = i;
        }
    }

    if (first < 0) {
        return {};
    }

    std::string result;
    result.reserve(static_cast<size_t>(last - first + 1));

    for (int i = first; i <= last; ++i) {
        const unsigned char c = static_cast<unsigned char>(word[i]);
        if (std::isalnum(c)) {
            result += static_cast<char>(std::tolower(c));
        } else if (word[i] == '\'' && i > first && i < last) {
            result += '\'';
        }
    }

    return result;
}

bool LyricToken::alignable() const {
    return !normalized.empty();
}

bool LyricLine::blank() const {
    return tokens.empty();
}

std::vector<const LyricToken*> LyricSheet::alignableTokens() const {
    std::vector<const LyricToken*> result;
    for (const LyricLine& line : lines) {
        for (const LyricToken& tok : line.tokens) {
            if (tok.alignable()) {
                result.push_back(&tok);
            }
        }
    }
    return result;
}

LyricSheet parseLyrics(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open lyrics file: " + path);
    }

    LyricSheet sheet;
    int lineIndex   = 0;
    int globalIndex = 0;
    bool hasContent = false;

    std::string rawLine;
    while (std::getline(file, rawLine)) {
        LyricLine lyricLine;
        lyricLine.text      = rawLine;
        lyricLine.lineIndex = lineIndex;

        const std::vector<std::string> words = splitOnWhitespace(rawLine);
        int tokenInLine = 0;

        for (const std::string& word : words) {
            LyricToken tok;
            tok.text        = word;
            tok.normalized  = isPunctuationOnly(word) ? "" : normalizeLyricWord(word);
            tok.lineIndex   = lineIndex;
            tok.tokenInLine = tokenInLine;
            tok.globalIndex = globalIndex;

            lyricLine.tokens.push_back(std::move(tok));
            ++tokenInLine;
            ++globalIndex;
            hasContent = true;
        }

        sheet.lines.push_back(std::move(lyricLine));
        ++lineIndex;
    }

    if (!hasContent) {
        throw std::runtime_error("Lyrics file is empty or contains no words: " + path);
    }

    return sheet;
}

std::vector<Token> alignLyricsToWhisper(
    const LyricSheet& sheet,
    const std::vector<Token>& whisperTokens
) {
    struct WhisperEntry {
        std::string normalized;
        int startMs;
        int endMs;
    };

    std::vector<WhisperEntry> wEntries;
    wEntries.reserve(whisperTokens.size());
    for (const Token& t : whisperTokens) {
        wEntries.push_back({normalizeLyricWord(t.text), t.startMs, t.endMs});
    }

    struct MatchResult {
        std::string text;
        int lineIndex;
        int startMs = 0;
        int endMs   = 0;
        bool matched = false;
    };

    std::vector<MatchResult> results;

    static const int kLookahead = 8;
    int wCursor = 0;

    for (const LyricLine& line : sheet.lines) {
        for (const LyricToken& tok : line.tokens) {
            if (!tok.alignable()) continue;

            MatchResult r;
            r.text      = tok.text;
            r.lineIndex = tok.lineIndex;

            const int wEnd = std::min(
                static_cast<int>(wEntries.size()),
                wCursor + kLookahead
            );

            for (int w = wCursor; w < wEnd; ++w) {
                if (wEntries[w].normalized == tok.normalized) {
                    r.startMs = wEntries[w].startMs;
                    r.endMs   = wEntries[w].endMs;
                    r.matched = true;
                    wCursor   = w + 1;
                    break;
                }
            }

            results.push_back(r);
        }
    }

    const int audioEndMs = wEntries.empty() ? 0 : wEntries.back().endMs;
    const int n = static_cast<int>(results.size());

    for (int i = 0; i < n; ) {
        if (results[i].matched) { ++i; continue; }

        int j = i;
        while (j < n && !results[j].matched) ++j;

        const int prevEnd   = (i > 0) ? results[i - 1].endMs : 0;
        const int nextStart = (j < n) ? results[j].startMs   : audioEndMs;
        const int count     = j - i;
        const int step      = (count > 0) ? (nextStart - prevEnd) / count : 0;

        for (int k = i; k < j; ++k) {
            results[k].startMs = prevEnd + step * (k - i);
            results[k].endMs   = prevEnd + step * (k - i + 1);
        }

        i = j;
    }

    std::vector<Token> tokens;
    tokens.reserve(results.size());
    for (int i = 0; i < n; ++i) {
        tokens.emplace_back(
            i,
            results[i].text,
            results[i].startMs,
            results[i].endMs,
            results[i].lineIndex,
            std::nullopt
        );
    }

    return tokens;
}
