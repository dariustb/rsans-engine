#include <rsans_data.h>

#include <nlohmann/json.hpp>
#include <sndfile.h>

#include <optional>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

double getAudioDuration(const std::string& audioPath) {
    SF_INFO info{};
    SNDFILE* f = sf_open(audioPath.c_str(), SFM_READ, &info);
    if (!f) return -1.0;

    double seconds =
        static_cast<double>(info.frames) / info.samplerate;

    sf_close(f);
    return seconds;
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

ProjectData::ProjectData(ProjectData&& other) noexcept
: audio(other.audio)
, video(other.video)
, layout(other.layout)
, model(other.model)
, cmudict(std::move(other.cmudict))
, tokens(std::move(other.tokens))
, colorSwatch(std::move(other.colorSwatch))
{}

std::string ProjectData::toJson() const {
    json j;

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
