#include <rsans_fonts.h>

#include <fontconfig/fontconfig.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string getString(FcPattern* pattern, const char* property) {
    FcChar8* value = nullptr;
    if (FcPatternGetString(pattern, property, 0, &value) == FcResultMatch && value != nullptr) {
        return reinterpret_cast<const char*>(value);
    }
    return "";
}

int getInteger(FcPattern* pattern, const char* property, int fallback) {
    int value = 0;
    return FcPatternGetInteger(pattern, property, 0, &value) == FcResultMatch ? value : fallback;
}

// libass (through FreeType) renders TrueType and CFF (OpenType) outlines
bool isUsableFormat(const std::string& format) {
    return format == "TrueType" || format == "CFF";
}

// Lower is better: how far a face is from upright, normal weight
int regularScore(FcPattern* pattern) {
    const int slant = getInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
    const int weight = getInteger(pattern, FC_WEIGHT, FC_WEIGHT_REGULAR);
    return (slant == FC_SLANT_ROMAN ? 0 : 1000) + std::abs(weight - FC_WEIGHT_REGULAR);
}

struct PatternDeleter { void operator()(FcPattern* p) const { FcPatternDestroy(p); } };
struct ObjectSetDeleter { void operator()(FcObjectSet* s) const { FcObjectSetDestroy(s); } };
struct FontSetDeleter { void operator()(FcFontSet* s) const { FcFontSetDestroy(s); } };

} // namespace

std::vector<FontInfo> scanSystemFonts() {
    if (FcInit() == FcFalse) {
        return {};
    }

    const std::unique_ptr<FcPattern, PatternDeleter> pattern(FcPatternCreate());
    const std::unique_ptr<FcObjectSet, ObjectSetDeleter> properties(FcObjectSetBuild(
        FC_FAMILY, FC_STYLE, FC_FILE, FC_FONTFORMAT, FC_SLANT, FC_WEIGHT, nullptr));
    const std::unique_ptr<FcFontSet, FontSetDeleter> fonts(
        FcFontList(nullptr, pattern.get(), properties.get()));
    if (!fonts) {
        return {};
    }

    // Keep the most regular face of each family
    struct Candidate {
        FontInfo font;
        int score;
    };
    std::map<std::string, Candidate> byFamily;  // Keyed by lowercase family
    for (int i = 0; i < fonts->nfont; ++i) {
        FcPattern* font = fonts->fonts[i];
        if (!isUsableFormat(getString(font, FC_FONTFORMAT))) {
            continue;
        }
        FontInfo info{getString(font, FC_FAMILY), getString(font, FC_STYLE), getString(font, FC_FILE)};
        if (info.family.empty() || info.path.empty()) {
            continue;
        }
        const int score = regularScore(font);
        const std::string key = toLower(info.family);
        const auto existing = byFamily.find(key);
        if (existing == byFamily.end() || score < existing->second.score ||
            (score == existing->second.score && info.path < existing->second.font.path)) {
            byFamily[key] = Candidate{std::move(info), score};
        }
    }

    std::vector<FontInfo> result;
    result.reserve(byFamily.size());
    for (auto& [key, candidate] : byFamily) {
        result.push_back(std::move(candidate.font));
    }
    return result;  // std::map keeps them sorted by lowercase family
}

std::optional<FontInfo> fontFromFile(const std::string& path) {
    int faceCount = 0;
    const std::unique_ptr<FcPattern, PatternDeleter> font(FcFreeTypeQuery(
        reinterpret_cast<const FcChar8*>(path.c_str()), 0, nullptr, &faceCount));
    if (!font || !isUsableFormat(getString(font.get(), FC_FONTFORMAT))) {
        return std::nullopt;
    }
    FontInfo info{getString(font.get(), FC_FAMILY), getString(font.get(), FC_STYLE), path};
    if (info.family.empty()) {
        return std::nullopt;
    }
    return info;
}

std::vector<std::size_t> filterFonts(const std::vector<FontInfo>& fonts, const std::string& query) {
    std::vector<std::string> words;
    std::istringstream stream(toLower(query));
    std::string word;
    while (stream >> word) {
        words.push_back(word);
    }

    std::vector<std::size_t> startsWith;
    std::vector<std::size_t> containsAll;
    for (std::size_t i = 0; i < fonts.size(); ++i) {
        const std::string family = toLower(fonts[i].family);
        const bool matches = std::all_of(words.begin(), words.end(), [&](const std::string& w) {
            return family.find(w) != std::string::npos;
        });
        if (!matches) {
            continue;
        }
        if (!words.empty() && family.rfind(words.front(), 0) == 0) {
            startsWith.push_back(i);
        } else {
            containsAll.push_back(i);
        }
    }
    startsWith.insert(startsWith.end(), containsAll.begin(), containsAll.end());
    return startsWith;
}
