#include <rsans_data.h>

#include <nlohmann/json.hpp>

#include <vector>

using json = nlohmann::json;

ProjectData::ProjectData(const std::string& jsonContent) {
    const json j = json::parse(jsonContent);

    audio.path = j["audio"]["path"].get<std::string>();
    audio.length = j["audio"]["length"].get<int>();

    video.width = j["video"]["width"].get<int>();
    video.height = j["video"]["height"].get<int>();
    video.background = j["video"]["background"].get<std::string>();

    layout.fontName = j["layout"]["fontName"].get<std::string>();
    layout.fontSize = j["layout"]["fontSize"].get<int>();
    layout.lineHeight = j["layout"]["lineHeight"].get<int>();

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
, tokens(other.tokens) {}
