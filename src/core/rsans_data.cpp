#include <rsans_data.h>

#include <nlohmann/json.hpp>
#include <sndfile.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
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

ProjectData::ProjectData(ProjectData base, std::vector<Token>& newTokens)
: ProjectData(std::move(base)) {
    tokens = std::move(newTokens);
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

std::vector<ValidationError> ProjectData::validate() const {
    std::vector<ValidationError> errors;
    const auto addError = [&](const std::string& field, const std::string& message) {
        errors.push_back({field, message});
    };

    // Mandatory: must be set to a usable value
    const auto requireText = [&](const std::string& field, const std::string& value) {
        if (value.empty()) {
            addError(field, "is required");
        }
    };
    const auto requireFile = [&](const std::string& field, const std::string& path) {
        if (path.empty()) {
            addError(field, "is required");
        } else if (!isExistingFile(path)) {
            addError(field, "file not found: " + path);
        }
    };
    const auto requirePositive = [&](const std::string& field, int value) {
        if (value <= 0) {
            addError(field, "must be greater than 0");
        }
    };

    // header
    requireText("header.fontName", header.fontName);
    requireFile("header.fontPath", header.fontPath);
    requireText("header.title", header.title);
    requirePositive("header.titleSize", header.titleSize);
    requireText("header.artist", header.artist);
    requirePositive("header.artistSize", header.artistSize);
    requireFile("header.media", header.media);

    // audio
    requireFile("audio.path", audio.path);
    if (audio.length <= 0.0) {
        addError("audio.length", "must be greater than 0");
    }

    // video (libx264's yuv420p output needs even dimensions)
    requirePositive("video.width", video.width);
    requirePositive("video.height", video.height);
    if (video.width > 0 && video.width % 2 != 0) {
        addError("video.width", "must be an even number");
    }
    if (video.height > 0 && video.height % 2 != 0) {
        addError("video.height", "must be an even number");
    }
    requireText("video.background", video.background);

    // layout
    requireText("layout.fontName", layout.fontName);
    requireFile("layout.fontPath", layout.fontPath);
    requirePositive("layout.fontSize", layout.fontSize);
    requirePositive("layout.lineHeight", layout.lineHeight);

    // model
    requireFile("model.path", model.base);

    // Optional: may be left empty, but must be usable if set
    if (!cmudict.empty() && !isExistingFile(cmudict)) {
        addError("cmudict", "file not found: " + cmudict);
    }

    for (size_t i = 0; i < colorSwatch.size(); ++i) {
        if (!isHexColor(colorSwatch[i].hex())) {
            addError("colorSwatch[" + std::to_string(i) + "]",
                     "must be a #RRGGBB color, got: " + colorSwatch[i].hex());
        }
    }

    bool hasRhymes = false;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens[i];
        const std::string field = "tokens[" + std::to_string(i) + "]";
        if (token.text.empty()) {
            addError(field + ".text", "must not be empty");
        }
        if (token.startMs < 0) {
            addError(field + ".startMs", "must not be negative");
        }
        if (token.endMs < token.startMs) {
            addError(field + ".endMs", "must not be before startMs");
        }
        if (token.lineIndex < 0) {
            addError(field + ".lineIndex", "must not be negative");
        }
        if (token.rhymeIndex.has_value()) {
            hasRhymes = true;
            if (token.rhymeIndex.value() < 0) {
                addError(field + ".rhymeIndex", "must not be negative");
            }
        }
    }

    // Rhyme groups are colored from the swatch, so one is needed once rhymes exist
    if (hasRhymes && colorSwatch.empty()) {
        addError("colorSwatch", "needs at least one color when tokens have rhyme groups");
    }

    return errors;
}

bool ProjectData::isValid() const {
    return validate().empty();
}
