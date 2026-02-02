#include <optional>
#include <rsans_data.h>

#include <nlohmann/json.hpp>
#include <sndfile.h>

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

Token::Token(int id, const std::string& text,int startMs, int endMs, int lineIndex, const std::optional<std::string>& rhymeGroup)
: id(id)
, text(text)
, startMs(startMs)
, endMs(endMs)
, lineIndex(lineIndex)
, rhymeGroup(rhymeGroup)
{}

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
        const std::optional<std::string> tokenRhymeGroup = tokenJson["rhymeGroup"].is_null()
            ? std::optional<std::string>{}
            : tokenJson["rhymeGroup"].get<std::string>();

        const Token token(
            tokenJson["id"].get<int>(),
            tokenJson["text"].get<std::string>(),
            tokenJson["startMs"].get<int>(),
            tokenJson["endMs"].get<int>(),
            tokenJson["lineIndex"].get<int>(),
            tokenRhymeGroup
        );

        tokens.push_back(token);
    }

    for (const auto& [key, value] : j["rhymeStyles"].items()) {
        RhymeStyle style;
        style.color = value["color"].get<std::string>();
        rhymeStyles[key] = style;
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
, rhymeStyles(std::move(other.rhymeStyles))
, tokens(std::move(other.tokens))
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
        tokenJson["rhymeGroup"] = token.rhymeGroup.has_value()
            ? json(token.rhymeGroup.value())
            : json(nullptr);
        j["tokens"].push_back(tokenJson);
    }

    j["rhymeStyles"] = json::object();
    for (const auto& [key, style] : rhymeStyles) {
        j["rhymeStyles"][key]["color"] = style.color;
    }

    return j.dump();
}
