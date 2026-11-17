#include <rsans_data.h>

#include <nlohmann/json.hpp>
#include <sndfile.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using json = nlohmann::json;

double getAudioDuration(const std::string& audioPath) {
    SF_INFO info{};
    SNDFILE* f = sf_open(audioPath.c_str(), SFM_READ, &info);
    if (!f) return -1.0;

    double seconds =
        static_cast<double>(info.frames) / info.samplerate;

    sf_close(f);
    return seconds;
}

namespace {

bool isExistingFile(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

bool isHexColor(const std::string& value) {
    if (value.size() != 7 || value[0] != '#') {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(),
                       [](unsigned char c) { return std::isxdigit(c) != 0; });
}

}

Token::Token(int id, const std::string& text,int startMs, int endMs, int lineIndex, const std::optional<int>& rhymeIndex)
: id(id)
, text(text)
, startMs(startMs)
, endMs(endMs)
, lineIndex(lineIndex)
, rhymeIndex(rhymeIndex)
{}

Color::Color(const std::string& hexValue)
: d_hex(hexValue)
, d_ass(toAss(hexValue))
{}

std::string Color::hex() const {
    return d_hex;
}

std::string Color::ass() const {
    return d_ass;
}

std::string Color::toAss(const std::string& hexValue) {
    const std::string hex = hexValue.substr(1);
    const std::string r = hex.substr(0, 2);
    const std::string g = hex.substr(2, 2);
    const std::string b = hex.substr(4, 2);
    return "&H00" + b + g + r;
}

ProjectData::ProjectData(const std::string& jsonContent) {
    const json j = json::parse(jsonContent);

    header.fontName = j["header"]["fontName"].get<std::string>();
    header.fontPath = j["header"]["fontPath"].get<std::string>();
    header.title = j["header"]["title"].get<std::string>();
    header.titleSize = j["header"]["titleSize"].get<int>();
    header.artist = j["header"]["artist"].get<std::string>();
    header.artistSize = j["header"]["artistSize"].get<int>();
    header.media = j["header"]["media"].get<std::string>();

    audio.path = j["audio"]["path"].get<std::string>();
    audio.length = j["audio"]["length"].is_null()
        ? getAudioDuration(audio.path)
        : j["audio"]["length"].get<double>();

    video.width = j["video"]["width"].get<int>();
    video.height = j["video"]["height"].get<int>();
    video.background = j["video"]["background"].get<std::string>();

    layout.fontName = j["layout"]["fontName"].get<std::string>();
    layout.fontPath = j["layout"]["fontPath"].get<std::string>();
    layout.fontSize = j["layout"]["fontSize"].get<int>();
    layout.lineHeight = j["layout"]["lineHeight"].get<int>();
    
    model.base = j["model"]["path"];

    if (j.contains("cmudict") && !j["cmudict"].is_null()) {
        cmudict = j["cmudict"].get<std::string>();
    }

    for (const auto& tokenJson : j["tokens"]) {    
        const std::optional<int> tokenRhymeIndex = tokenJson["rhymeIndex"].is_null()
            ? std::optional<int>{}
            : tokenJson["rhymeIndex"].get<int>();

        const Token token(
            tokenJson["id"].get<int>(),
            tokenJson["text"].get<std::string>(),
            tokenJson["startMs"].get<int>(),
            tokenJson["endMs"].get<int>(),
            tokenJson["lineIndex"].get<int>(),
            tokenRhymeIndex
        );

        tokens.push_back(token);
    }

    for (const std::string& colorHex : j["colorSwatch"]) {
        const Color color(colorHex);
        colorSwatch.push_back(color);
    }
}

std::string ProjectData::toJson() const {
    json j;

    j["header"]["fontName"] = header.fontName;
    j["header"]["fontPath"] = header.fontPath;
    j["header"]["title"] = header.title;
    j["header"]["titleSize"] = header.titleSize;
    j["header"]["artist"] = header.artist;
    j["header"]["artistSize"] = header.artistSize;
    j["header"]["media"] = header.media;

    j["audio"]["path"] = audio.path;
    j["audio"]["length"] = audio.length;

    j["video"]["width"] = video.width;
    j["video"]["height"] = video.height;
    j["video"]["background"] = video.background;

    j["layout"]["fontName"] = layout.fontName;
    j["layout"]["fontPath"] = layout.fontPath;
    j["layout"]["fontSize"] = layout.fontSize;
    j["layout"]["lineHeight"] = layout.lineHeight;

    j["model"]["path"] = model.base;

    if (!cmudict.empty()) {
        j["cmudict"] = cmudict;
    } else {
        j["cmudict"] = nullptr;
    }

    j["tokens"] = json::array();
    for (const auto& token : tokens) {
        json tokenJson;
        tokenJson["id"] = token.id;
        tokenJson["text"] = token.text;
        tokenJson["startMs"] = token.startMs;
        tokenJson["endMs"] = token.endMs;
        tokenJson["lineIndex"] = token.lineIndex;
        tokenJson["rhymeIndex"] = token.rhymeIndex.has_value()
            ? json(token.rhymeIndex.value())
            : json(nullptr);
        j["tokens"].push_back(tokenJson);
    }

    j["colorSwatch"] = json::array();
    for (const auto& color : colorSwatch) {
        j["colorSwatch"].push_back(color.hex());
    }

    return j.dump();
}

namespace {

// Collects validation errors. Each check* function covers one part of the
// project, so the project-wide and per-step validations can share them.
class Validator {
  private:
    std::vector<ValidationError> d_errors;

  public:
    void addError(const std::string& field, const std::string& message) {
        d_errors.push_back({field, message});
    }

    void requireText(const std::string& field, const std::string& value) {
        if (value.empty()) {
            addError(field, "is required");
        }
    }

    void requireFile(const std::string& field, const std::string& path) {
        if (path.empty()) {
            addError(field, "is required");
        } else if (!isExistingFile(path)) {
            addError(field, "file not found: " + path);
        }
    }

    void optionalFile(const std::string& field, const std::string& path) {
        if (!path.empty() && !isExistingFile(path)) {
            addError(field, "file not found: " + path);
        }
    }

    void requirePositive(const std::string& field, int value) {
        if (value <= 0) {
            addError(field, "must be greater than 0");
        }
    }

    std::vector<ValidationError> errors() && {
        return std::move(d_errors);
    }
};

void checkHeader(const ProjectData& p, Validator& v) {
    v.requireText("header.fontName", p.header.fontName);
    v.requireFile("header.fontPath", p.header.fontPath);
    v.requireText("header.title", p.header.title);
    v.requirePositive("header.titleSize", p.header.titleSize);
    v.requireText("header.artist", p.header.artist);
    v.requirePositive("header.artistSize", p.header.artistSize);
    v.requireFile("header.media", p.header.media);
}

void checkAudioPath(const ProjectData& p, Validator& v) {
    v.requireFile("audio.path", p.audio.path);
}

void checkAudioLength(const ProjectData& p, Validator& v) {
    if (p.audio.length <= 0.0) {
        v.addError("audio.length", "must be greater than 0");
    }
}

void checkVideo(const ProjectData& p, Validator& v) {
    // libx264's yuv420p output needs even dimensions
    v.requirePositive("video.width", p.video.width);
    v.requirePositive("video.height", p.video.height);
    if (p.video.width > 0 && p.video.width % 2 != 0) {
        v.addError("video.width", "must be an even number");
    }
    if (p.video.height > 0 && p.video.height % 2 != 0) {
        v.addError("video.height", "must be an even number");
    }
    v.requireText("video.background", p.video.background);
}

void checkLayout(const ProjectData& p, Validator& v) {
    v.requireText("layout.fontName", p.layout.fontName);
    v.requireFile("layout.fontPath", p.layout.fontPath);
    v.requirePositive("layout.fontSize", p.layout.fontSize);
    v.requirePositive("layout.lineHeight", p.layout.lineHeight);
}

void checkModel(const ProjectData& p, Validator& v) {
    v.requireFile("model.path", p.model.base);
}

void checkCmudict(const ProjectData& p, Validator& v, bool required) {
    if (required) {
        v.requireFile("cmudict", p.cmudict);
    } else {
        v.optionalFile("cmudict", p.cmudict);
    }
}

void checkColorSwatch(const ProjectData& p, Validator& v) {
    for (size_t i = 0; i < p.colorSwatch.size(); ++i) {
        if (!isHexColor(p.colorSwatch[i].hex())) {
            v.addError("colorSwatch[" + std::to_string(i) + "]",
                       "must be a #RRGGBB color, got: " + p.colorSwatch[i].hex());
        }
    }
}

void checkTokens(const ProjectData& p, Validator& v, bool required) {
    if (required && p.tokens.empty()) {
        v.addError("tokens", "no lyrics yet; run analyze first");
    }

    bool hasRhymes = false;
    for (size_t i = 0; i < p.tokens.size(); ++i) {
        const Token& token = p.tokens[i];
        const std::string field = "tokens[" + std::to_string(i) + "]";
        if (token.text.empty()) {
            v.addError(field + ".text", "must not be empty");
        }
        if (token.startMs < 0) {
            v.addError(field + ".startMs", "must not be negative");
        }
        if (token.endMs < token.startMs) {
            v.addError(field + ".endMs", "must not be before startMs");
        }
        if (token.lineIndex < 0) {
            v.addError(field + ".lineIndex", "must not be negative");
        }
        if (token.rhymeIndex.has_value()) {
            hasRhymes = true;
            if (token.rhymeIndex.value() < 0) {
                v.addError(field + ".rhymeIndex", "must not be negative");
            }
        }
    }

    // Rhyme groups are colored from the swatch, so one is needed once rhymes exist
    if (hasRhymes && p.colorSwatch.empty()) {
        v.addError("colorSwatch", "needs at least one color when tokens have rhyme groups");
    }
}

} // namespace

std::vector<ValidationError> ProjectData::validate() const {
    Validator v;
    checkHeader(*this, v);
    checkAudioPath(*this, v);
    checkAudioLength(*this, v);
    checkVideo(*this, v);
    checkLayout(*this, v);
    checkModel(*this, v);
    checkCmudict(*this, v, false);
    checkColorSwatch(*this, v);
    checkTokens(*this, v, false);
    return std::move(v).errors();
}

bool ProjectData::isValid() const {
    return validate().empty();
}

std::vector<ValidationError> ProjectData::validateForAnalyze() const {
    Validator v;
    checkAudioPath(*this, v);
    checkModel(*this, v);
    return std::move(v).errors();
}

std::vector<ValidationError> ProjectData::validateForRhyme() const {
    Validator v;
    checkCmudict(*this, v, true);
    checkTokens(*this, v, true);
    return std::move(v).errors();
}

std::vector<ValidationError> ProjectData::validateForExport() const {
    Validator v;
    checkHeader(*this, v);
    checkAudioPath(*this, v);
    checkAudioLength(*this, v);
    checkVideo(*this, v);
    checkLayout(*this, v);
    checkColorSwatch(*this, v);
    checkTokens(*this, v, true);
    return std::move(v).errors();
}

std::vector<ValidationError> ProjectData::validateForFull() const {
    // Everything analyze, rhyme, and export need, except tokens, which
    // analyze produces. Rhymes will be found, so a swatch is needed too.
    Validator v;
    checkHeader(*this, v);
    checkAudioPath(*this, v);
    checkAudioLength(*this, v);
    checkVideo(*this, v);
    checkLayout(*this, v);
    checkModel(*this, v);
    checkCmudict(*this, v, true);
    checkColorSwatch(*this, v);
    if (colorSwatch.empty()) {
        v.addError("colorSwatch", "needs at least one color to highlight rhymes");
    }
    return std::move(v).errors();
}
