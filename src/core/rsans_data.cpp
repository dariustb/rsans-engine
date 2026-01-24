#include <rsans_data.h>

#include <nlohmann/json.hpp>

#include <vector>

using json = nlohmann::json;

ProjectData::ProjectData(const std::string& jsonContent) {
    const json j = json::parse(jsonContent);

    audio.path = j["audio"]["path"].get<std::string>();
    audio.length = j["audio"]["length"].get<double>();

    video.width = j["video"]["width"].get<int>();
    video.height = j["video"]["height"].get<int>();
    video.background = j["video"]["background"].get<std::string>();

    layout.fontName = j["layout"]["fontName"].get<std::string>();
    layout.fontSize = j["layout"]["fontSize"].get<int>();
    layout.lineHeight = j["layout"]["lineHeight"].get<int>();
    
    model.base = j["model"]["path"];

    for (const auto& tokenJson : j["tokens"]) {
        Token token;
        token.id = tokenJson["id"].get<int>();
        token.text = tokenJson["text"].get<std::string>();
        token.startMs = tokenJson["startMs"].get<int>();
        token.endMs = tokenJson["endMs"].get<int>();
        token.lineIndex = tokenJson["lineIndex"].get<int>();

        if (tokenJson["rhymeGroup"].is_null()) {
            token.rhymeGroup = std::nullopt;
        } else {
            token.rhymeGroup = tokenJson["rhymeGroup"].get<std::string>();
        }

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
, rhymeStyles(other.rhymeStyles)
, model(other.model)
, tokens(other.tokens) {}

std::string ProjectData::toJson() const {
    json j;

    j["audio"]["path"] = audio.path;
    j["audio"]["length"] = audio.length;

    j["video"]["width"] = video.width;
    j["video"]["height"] = video.height;
    j["video"]["background"] = video.background;

    j["layout"]["fontName"] = layout.fontName;
    j["layout"]["fontSize"] = layout.fontSize;
    j["layout"]["lineHeight"] = layout.lineHeight;

    j["model"]["path"] = model.base;

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
