#include <rsans_rhyme.h>

#include <rsans_data.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {

std::string toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string normalizeWord(const std::string& word) {
    std::string result;
    for (char c : word) {
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '\'') {
            result += std::tolower(static_cast<unsigned char>(c));
        }
    }
    return result;
}

std::string phonemeVecToString(const std::vector<std::string>& phonemes) {
    std::string result;
    for (size_t i = 0; i < phonemes.size(); ++i) {
        if (i > 0) result += " ";
        result += phonemes[i];
    }
    return result;
}

} // namespace

CMUDict::CMUDict(const std::string& dictPath) {
    std::ifstream file(dictPath);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments
        if (line.empty() || line[0] == ';') {
            continue;
        }

        // Skip lines starting with punctuation that isn't apostrophe
        if (!line.empty() && !std::isalpha(static_cast<unsigned char>(line[0])) && line[0] != '\'') {
            // Check if it's a valid word line
            bool hasAlpha = false;
            for (char c : line) {
                if (std::isalpha(static_cast<unsigned char>(c))) {
                    hasAlpha = true;
                    break;
                }
                if (c == ' ' || c == '\t') break;
            }
            if (!hasAlpha && line[0] != '\'') continue;
        }

        std::istringstream iss(line);
        std::string word;
        iss >> word;

        if (word.empty()) continue;

        // Strip comments (# and everything after)
        std::vector<std::string> phonemes;
        std::string phoneme;
        while (iss >> phoneme) {
            if (phoneme[0] == '#') break;
            phonemes.push_back(phoneme);
        }

        if (phonemes.empty()) continue;

        // Handle alternate pronunciations: word(2), word(3), etc.
        std::string baseWord = word;
        size_t parenPos = word.find('(');
        if (parenPos != std::string::npos) {
            baseWord = word.substr(0, parenPos);
        }

        // Store with lowercase key (keep first pronunciation if multiple)
        std::string key = toLower(baseWord);
        if (d_entries.find(key) == d_entries.end()) {
            d_entries[key] = phonemes;
        }
    }
}

std::vector<std::string> CMUDict::getPhonemes(const std::string& word) const {
    std::string key = toLower(word);
    auto it = d_entries.find(key);
    if (it != d_entries.end()) {
        return it->second;
    }
    return {};
}

bool CMUDict::isVowel(const std::string& phoneme) {
    // CMU vowels end with stress markers 0, 1, or 2
    if (phoneme.empty()) return false;
    char last = phoneme.back();
    return last == '0' || last == '1' || last == '2';
}

std::vector<std::string> CMUDict::extractRhymeTail(const std::vector<std::string>& phonemes) {
    if (phonemes.empty()) return {};

    // Find the last primary stressed vowel (stress marker 1)
    int lastPrimaryStress = -1;
    int lastSecondaryStress = -1;
    int lastVowel = -1;

    for (int i = static_cast<int>(phonemes.size()) - 1; i >= 0; --i) {
        if (isVowel(phonemes[i])) {
            if (lastVowel < 0) lastVowel = i;
            char stress = phonemes[i].back();
            if (stress == '1' && lastPrimaryStress < 0) {
                lastPrimaryStress = i;
            } else if (stress == '2' && lastSecondaryStress < 0) {
                lastSecondaryStress = i;
            }
        }
    }

    // Prefer primary stress, fallback to secondary, fallback to any vowel
    int startIdx = lastPrimaryStress;
    if (startIdx < 0) startIdx = lastSecondaryStress;
    if (startIdx < 0) startIdx = lastVowel;
    if (startIdx < 0) return {};

    return std::vector<std::string>(phonemes.begin() + startIdx, phonemes.end());
}

std::vector<std::string> CMUDict::extractVowels(const std::vector<std::string>& phonemes) {
    std::vector<std::string> vowels;
    for (const auto& p : phonemes) {
        if (isVowel(p)) {
            // Strip stress marker for comparison
            std::string base = p.substr(0, p.size() - 1);
            vowels.push_back(base);
        }
    }
    return vowels;
}

std::vector<std::string> CMUDict::extractConsonants(const std::vector<std::string>& phonemes) {
    std::vector<std::string> consonants;
    for (const auto& p : phonemes) {
        if (!isVowel(p)) {
            consonants.push_back(p);
        }
    }
    return consonants;
}

int RhymeGrouper::find(int x) {
    if (parent_.find(x) == parent_.end()) {
        parent_[x] = x;
        rank_[x] = 0;
    }
    if (parent_[x] != x) {
        parent_[x] = find(parent_[x]); // Path compression
    }
    return parent_[x];
}

void RhymeGrouper::unite(int x, int y) {
    int rootX = find(x);
    int rootY = find(y);
    if (rootX == rootY) return;

    // Union by rank
    if (rank_[rootX] < rank_[rootY]) {
        parent_[rootX] = rootY;
    } else if (rank_[rootX] > rank_[rootY]) {
        parent_[rootY] = rootX;
    } else {
        parent_[rootY] = rootX;
        rank_[rootX]++;
    }
}

ProjectData detectRhymes(const ProjectData& project) {
    CMUDict dict(project.cmudict);

    // Copy the project to modify tokens
    ProjectData result(project.toJson());

    if (result.tokens.empty()) {
        return result;
    }

    // Extract rhyme tails for each token
    struct TokenRhymeInfo {
        int tokenIdx;
        std::vector<std::string> rhymeTail;
        std::vector<std::string> vowelsOnly;
    };

    std::vector<TokenRhymeInfo> rhymeInfos;

    for (size_t i = 0; i < result.tokens.size(); ++i) {
        const Token& token = result.tokens[i];
        std::string normalized = normalizeWord(token.text);

        if (normalized.empty()) continue;

        std::vector<std::string> phonemes = dict.getPhonemes(normalized);
        if (phonemes.empty()) continue;

        std::vector<std::string> tail = CMUDict::extractRhymeTail(phonemes);
        if (tail.empty()) continue;

        TokenRhymeInfo info;
        info.tokenIdx = static_cast<int>(i);
        info.rhymeTail = tail;
        info.vowelsOnly = CMUDict::extractVowels(tail);

        rhymeInfos.push_back(info);
    }

    if (rhymeInfos.size() < 2) {
        return result;
    }

    // Build indexes for grouping
    // Map rhyme tail string -> list of token indices in rhymeInfos
    std::unordered_map<std::string, std::vector<int>> perfectRhymeMap;
    std::unordered_map<std::string, std::vector<int>> vowelMap;

    for (size_t i = 0; i < rhymeInfos.size(); ++i) {
        const auto& info = rhymeInfos[i];

        std::string tailKey = phonemeVecToString(info.rhymeTail);
        std::string vowelKey = phonemeVecToString(info.vowelsOnly);

        perfectRhymeMap[tailKey].push_back(static_cast<int>(i));

        if (!info.vowelsOnly.empty()) {
            vowelMap[vowelKey].push_back(static_cast<int>(i));
        }
    }

    // Use union-find to merge groups
    RhymeGrouper grouper;

    // Perfect rhymes: unite all tokens with same rhyme tail
    for (const auto& [key, indices] : perfectRhymeMap) {
        for (size_t i = 1; i < indices.size(); ++i) {
            grouper.unite(indices[0], indices[i]);
        }
    }

    // Slant rhymes (assonance): unite tokens with same vowel pattern
    for (const auto& [key, indices] : vowelMap) {
        for (size_t i = 1; i < indices.size(); ++i) {
            grouper.unite(indices[0], indices[i]);
        }
    }

    // Collect final groups and filter to groups with 2+ members
    std::unordered_map<int, std::vector<int>> groupToTokens;
    for (size_t i = 0; i < rhymeInfos.size(); ++i) {
        int root = grouper.find(static_cast<int>(i));
        groupToTokens[root].push_back(rhymeInfos[i].tokenIdx);
    }

    // Assign group IDs to tokens
    int groupId = 0;
    for (const auto& [root, tokenIndices] : groupToTokens) {
        if (tokenIndices.size() < 2) {
            continue; // Skip singletons
        }

        for (int tokenIdx : tokenIndices) {
            result.tokens[tokenIdx].rhymeIndex = groupId;
        }
        groupId++;
    }

    return result;
}
