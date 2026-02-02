#include <rsans_data.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <sstream>

using json = nlohmann::json;

TEST(ProjectDataTest, ProjectDataConstructorParsesAudioConfigGivenValidJson) {
    // Given
    const std::string json = R"({
        "audio": {
            "path": "test.wav",
            "length": 42
        },
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": []
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.audio.path, "test.wav");
    EXPECT_EQ(data.audio.length, 42);
}

TEST(ProjectDataTest, ProjectDataConstructorParsesVideoConfigGivenValidJson) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {
            "width": 1280,
            "height": 720,
            "background": "#FFFFFF"
        },
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": []
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.video.width, 1280);
    EXPECT_EQ(data.video.height, 720);
    EXPECT_EQ(data.video.background, "#FFFFFF");
}

TEST(ProjectDataTest, ProjectDataConstructorParsesLayoutConfigGivenValidJson) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {
            "fontName": "Helvetica",
            "fontPath": "fonts/helvetica.ttf",
            "fontSize": 36,
            "lineHeight": 50
        },
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": []
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.layout.fontName, "Helvetica");
    EXPECT_EQ(data.layout.fontSize, 36);
    EXPECT_EQ(data.layout.lineHeight, 50);
}

TEST(ProjectDataTest, ProjectDataConstructorParsesTokensGivenValidJson) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": [
            {
                "id": 1,
                "text": "hello",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": 0,
                "rhymeIndex": null
            },
            {
                "id": 2,
                "text": "world",
                "startMs": 500,
                "endMs": 1000,
                "lineIndex": 0,
                "rhymeIndex": null
            }
        ]
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.tokens.size(), 2);
    EXPECT_EQ(data.tokens[0].id, 1);
    EXPECT_EQ(data.tokens[0].text, "hello");
    EXPECT_EQ(data.tokens[0].startMs, 0);
    EXPECT_EQ(data.tokens[0].endMs, 500);
    EXPECT_EQ(data.tokens[0].lineIndex, 0);
    EXPECT_FALSE(data.tokens[0].rhymeIndex.has_value());

    EXPECT_EQ(data.tokens[1].id, 2);
    EXPECT_EQ(data.tokens[1].text, "world");
    EXPECT_EQ(data.tokens[1].startMs, 500);
    EXPECT_EQ(data.tokens[1].endMs, 1000);
    EXPECT_EQ(data.tokens[1].lineIndex, 0);
    EXPECT_FALSE(data.tokens[1].rhymeIndex.has_value());
}

TEST(ProjectDataTest, ProjectDataConstructorParsesRhymeGroupGivenTokenWithRhyme) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {"A": {"color": "#FF0000"}},
        "colorSwatch": [],
        "tokens": [
            {
                "id": 1,
                "text": "cat",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": 0,
                "rhymeIndex": 0
            }
        ]
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.tokens.size(), 1);
    EXPECT_TRUE(data.tokens[0].rhymeIndex.has_value());
    EXPECT_EQ(data.tokens[0].rhymeIndex.value(), 0);
}


TEST(ProjectDataTest, ProjectDataConstructorHandlesEmptyTokensGivenEmptyArray) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": []
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.tokens.size(), 0);
}

TEST(ProjectDataTest, ProjectDataConstructorHandlesEmptyRhymeStylesGivenEmptyObject) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": []
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.rhymeStyles.size(), 0);
}

TEST(ProjectDataTest, ProjectDataConstructorParsesMixedRhymeGroupsGivenTokens) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {"A": {"color": "#FF0000"}},
        "colorSwatch": [],
        "tokens": [
            {
                "id": 1,
                "text": "cat",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": 0,
                "rhymeIndex": 0
            },
            {
                "id": 2,
                "text": "and",
                "startMs": 500,
                "endMs": 700,
                "lineIndex": 0,
                "rhymeIndex": null
            },
            {
                "id": 3,
                "text": "hat",
                "startMs": 700,
                "endMs": 1000,
                "lineIndex": 0,
                "rhymeIndex": 0
            }
        ]
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.tokens.size(), 3);
    EXPECT_TRUE(data.tokens[0].rhymeIndex.has_value());
    EXPECT_EQ(data.tokens[0].rhymeIndex.value(), 0);
    EXPECT_FALSE(data.tokens[1].rhymeIndex.has_value());
    EXPECT_TRUE(data.tokens[2].rhymeIndex.has_value());
    EXPECT_EQ(data.tokens[2].rhymeIndex.value(), 0);
}

TEST(ProjectDataTest, ProjectDataConstructorParsesMultipleLinesGivenDifferentLineIndices) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": [
            {
                "id": 1,
                "text": "line",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": 0,
                "rhymeIndex": null
            },
            {
                "id": 2,
                "text": "one",
                "startMs": 500,
                "endMs": 1000,
                "lineIndex": 1,
                "rhymeIndex": null
            },
            {
                "id": 3,
                "text": "two",
                "startMs": 1000,
                "endMs": 1500,
                "lineIndex": 2,
                "rhymeIndex": null
            }
        ]
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.tokens.size(), 3);
    EXPECT_EQ(data.tokens[0].lineIndex, 0);
    EXPECT_EQ(data.tokens[1].lineIndex, 1);
    EXPECT_EQ(data.tokens[2].lineIndex, 2);
}

TEST(ProjectDataTest, ProjectDataConstructorReplacesTokensGivenBaseAndNewTokens) {
    // Given
    const std::string json = R"({
        "audio": {"path": "original.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {"A": {"color": "#FF0000"}},
        "colorSwatch": [],
        "tokens": [
            {
                "id": 1,
                "text": "old",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": 0,
                "rhymeIndex": null
            }
        ]
    })";
    ProjectData base(json);
    std::vector<Token> newTokens = {
        {2, "new", 100, 600, 0, std::nullopt}
    };

    // When
    ProjectData data(std::move(base), newTokens);

    // Then
    EXPECT_EQ(data.tokens.size(), 1);
    EXPECT_EQ(data.tokens[0].id, 2);
    EXPECT_EQ(data.tokens[0].text, "new");
    EXPECT_EQ(data.audio.path, "original.wav");
    EXPECT_EQ(data.video.width, 1920);
    EXPECT_EQ(data.layout.fontName, "Arial");
}

TEST(ProjectDataTest, ProjectDataMoveConstructorPreservesDataGivenSource) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 42},
        "video": {"width": 1920, "height": 1080, "background": "#123456"},
        "layout": {"fontName": "Times", "fontPath": "fonts/times.ttf", "fontSize": 32, "lineHeight": 40},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {"X": {"color": "#ABCDEF"}},
        "colorSwatch": ["#ABCDEF"],
        "tokens": [
            {
                "id": 99,
                "text": "moved",
                "startMs": 10,
                "endMs": 20,
                "lineIndex": 5,
                "rhymeIndex": 0
            }
        ]
    })";
    ProjectData source(json);

    // When
    ProjectData data(std::move(source));

    // Then
    EXPECT_EQ(data.audio.path, "test.wav");
    EXPECT_EQ(data.audio.length, 42);
    EXPECT_EQ(data.video.width, 1920);
    EXPECT_EQ(data.video.height, 1080);
    EXPECT_EQ(data.video.background, "#123456");
    EXPECT_EQ(data.layout.fontName, "Times");
    EXPECT_EQ(data.layout.fontSize, 32);
    EXPECT_EQ(data.layout.lineHeight, 40);
    EXPECT_EQ(data.tokens.size(), 1);
    EXPECT_EQ(data.tokens[0].id, 99);
    EXPECT_EQ(data.tokens[0].text, "moved");
    EXPECT_EQ(data.colorSwatch.size(), 1);
}

TEST(ProjectDataTest, ProjectDataConstructorParsesLargeTokenArrayGivenManyTokens) {
    // Given
    std::ostringstream json;
    json << R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": [)";

    for (int i = 0; i < 100; ++i) {
        if (i > 0) json << ",";
        json << R"({
            "id": )" << i << R"(,
            "text": "token)" << i << R"(",
            "startMs": )" << (i * 100) << R"(,
            "endMs": )" << ((i + 1) * 100) << R"(,
            "lineIndex": 0,
            "rhymeIndex": null
        })";
    }
    json << "]}";

    // When
    const ProjectData data(json.str());

    // Then
    EXPECT_EQ(data.tokens.size(), 100);
    EXPECT_EQ(data.tokens[0].id, 0);
    EXPECT_EQ(data.tokens[99].id, 99);
    EXPECT_EQ(data.tokens[50].text, "token50");
}

TEST(ProjectDataTest, ProjectDataConstructorParsesSpecialCharactersGivenUtf8Text) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": [
            {
                "id": 1,
                "text": "café",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": 0,
                "rhymeIndex": null
            },
            {
                "id": 2,
                "text": "日本語",
                "startMs": 500,
                "endMs": 1000,
                "lineIndex": 0,
                "rhymeIndex": null
            }
        ]
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.tokens.size(), 2);
    EXPECT_EQ(data.tokens[0].text, "café");
    EXPECT_EQ(data.tokens[1].text, "日本語");
}

TEST(ProjectDataTest, ProjectDataConstructorParsesZeroValuesGivenValidJson) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 0},
        "video": {"width": 0, "height": 0, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 0, "lineHeight": 0},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": [
            {
                "id": 0,
                "text": "zero",
                "startMs": 0,
                "endMs": 0,
                "lineIndex": 0,
                "rhymeIndex": null
            }
        ]
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.audio.length, 0);
    EXPECT_EQ(data.video.width, 0);
    EXPECT_EQ(data.video.height, 0);
    EXPECT_EQ(data.layout.fontSize, 0);
    EXPECT_EQ(data.layout.lineHeight, 0);
    EXPECT_EQ(data.tokens[0].id, 0);
    EXPECT_EQ(data.tokens[0].startMs, 0);
    EXPECT_EQ(data.tokens[0].endMs, 0);
}

TEST(ProjectDataTest, ProjectDataConstructorParsesNegativeLineIndexGivenValidJson) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": [
            {
                "id": 1,
                "text": "negative",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": -1,
                "rhymeIndex": null
            }
        ]
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.tokens[0].lineIndex, -1);
}


TEST(ProjectDataTest, ProjectDataConstructorPreservesTokenOrderGivenSequentialTokens) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": [
            {"id": 3, "text": "third", "startMs": 0, "endMs": 100, "lineIndex": 0, "rhymeIndex": null},
            {"id": 1, "text": "first", "startMs": 100, "endMs": 200, "lineIndex": 0, "rhymeIndex": null},
            {"id": 2, "text": "second", "startMs": 200, "endMs": 300, "lineIndex": 0, "rhymeIndex": null}
        ]
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.tokens.size(), 3);
    EXPECT_EQ(data.tokens[0].id, 3);
    EXPECT_EQ(data.tokens[1].id, 1);
    EXPECT_EQ(data.tokens[2].id, 2);
}

TEST(ProjectDataTest, ToJsonProducesValidJsonGivenProjectData) {
    // Given
    const std::string inputJson = R"({
        "audio": {"path": "test.wav", "length": 42},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": []
    })";
    const ProjectData data(inputJson);

    // When
    const std::string result = data.toJson();

    // Then
    EXPECT_NO_THROW(const json j = json::parse(result));
}

TEST(ProjectDataTest, ToJsonPreservesAudioConfigGivenProjectData) {
    // Given
    const std::string inputJson = R"({
        "audio": {"path": "music.mp3", "length": 180.5},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": []
    })";
    const ProjectData data(inputJson);

    // When
    const std::string result = data.toJson();
    const json j = json::parse(result);

    // Then
    EXPECT_EQ(j["audio"]["path"], "music.mp3");
    EXPECT_EQ(j["audio"]["length"], 180.5);
}

TEST(ProjectDataTest, ToJsonPreservesVideoConfigGivenProjectData) {
    // Given
    const std::string inputJson = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1280, "height": 720, "background": "#FFFFFF"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": []
    })";
    const ProjectData data(inputJson);

    // When
    const std::string result = data.toJson();
    const json j = json::parse(result);

    // Then
    EXPECT_EQ(j["video"]["width"], 1280);
    EXPECT_EQ(j["video"]["height"], 720);
    EXPECT_EQ(j["video"]["background"], "#FFFFFF");
}

TEST(ProjectDataTest, ToJsonPreservesLayoutConfigGivenProjectData) {
    // Given
    const std::string inputJson = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Helvetica", "fontPath": "fonts/helvetica.ttf", "fontSize": 36, "lineHeight": 50},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": []
    })";
    const ProjectData data(inputJson);

    // When
    const std::string result = data.toJson();
    const json j = json::parse(result);

    // Then
    EXPECT_EQ(j["layout"]["fontName"], "Helvetica");
    EXPECT_EQ(j["layout"]["fontSize"], 36);
    EXPECT_EQ(j["layout"]["lineHeight"], 50);
}

TEST(ProjectDataTest, ToJsonPreservesModelPathGivenProjectData) {
    // Given
    const std::string inputJson = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/whisper-large.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": []
    })";
    const ProjectData data(inputJson);

    // When
    const std::string result = data.toJson();
    const json j = json::parse(result);

    // Then
    EXPECT_EQ(j["model"]["path"], "models/whisper-large.bin");
}

TEST(ProjectDataTest, ToJsonPreservesTokensGivenProjectDataWithTokens) {
    // Given
    const std::string inputJson = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": [
            {"id": 1, "text": "hello", "startMs": 0, "endMs": 500, "lineIndex": 0, "rhymeIndex": null},
            {"id": 2, "text": "world", "startMs": 500, "endMs": 1000, "lineIndex": 0, "rhymeIndex": null}
        ]
    })";
    const ProjectData data(inputJson);

    // When
    const std::string result = data.toJson();
    const json j = json::parse(result);

    // Then
    EXPECT_EQ(j["tokens"].size(), 2);
    EXPECT_EQ(j["tokens"][0]["id"], 1);
    EXPECT_EQ(j["tokens"][0]["text"], "hello");
    EXPECT_EQ(j["tokens"][0]["startMs"], 0);
    EXPECT_EQ(j["tokens"][0]["endMs"], 500);
    EXPECT_EQ(j["tokens"][0]["lineIndex"], 0);
    EXPECT_TRUE(j["tokens"][0]["rhymeIndex"].is_null());
    EXPECT_EQ(j["tokens"][1]["id"], 2);
    EXPECT_EQ(j["tokens"][1]["text"], "world");
}

TEST(ProjectDataTest, ToJsonPreservesRhymeGroupGivenTokenWithRhyme) {
    // Given
    const std::string inputJson = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {"A": {"color": "#FF0000"}},
        "colorSwatch": [],
        "tokens": [
            {"id": 1, "text": "cat", "startMs": 0, "endMs": 500, "lineIndex": 0, "rhymeIndex": 0}
        ]
    })";
    const ProjectData data(inputJson);

    // When
    const std::string result = data.toJson();
    const json j = json::parse(result);

    // Then
    EXPECT_EQ(j["tokens"][0]["rhymeIndex"], 0);
}


TEST(ProjectDataTest, ToJsonHandlesEmptyCollectionsGivenMinimalData) {
    // Given
    const std::string inputJson = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "rhymeStyles": {},
        "colorSwatch": [],
        "tokens": []
    })";
    const ProjectData data(inputJson);

    // When
    const std::string result = data.toJson();
    const json j = json::parse(result);

    // Then
    EXPECT_TRUE(j["tokens"].is_array());
    EXPECT_EQ(j["tokens"].size(), 0);
    EXPECT_TRUE(j["rhymeStyles"].is_object());
    EXPECT_EQ(j["rhymeStyles"].size(), 0);
}

TEST(ProjectDataTest, ToJsonRoundTripPreservesDataGivenCompleteProjectData) {
    // Given
    const std::string inputJson = R"({
        "audio": {"path": "song.wav", "length": 240},
        "video": {"width": 3840, "height": 2160, "background": "#123456"},
        "layout": {"fontName": "Roboto", "fontPath": "fonts/roboto.ttf", "fontSize": 64, "lineHeight": 80},
        "model": {"path": "models/large-v3.bin"},
        "rhymeStyles": {
            "X": {"color": "#AABBCC"},
            "Y": {"color": "#DDEEFF"}
        },
        "colorSwatch": ["#AABBCC", "#DDEEFF"],
        "tokens": [
            {"id": 1, "text": "first", "startMs": 100, "endMs": 200, "lineIndex": 0, "rhymeIndex": 0},
            {"id": 2, "text": "second", "startMs": 200, "endMs": 400, "lineIndex": 1, "rhymeIndex": null}
        ]
    })";
    const ProjectData original(inputJson);

    // When
    const std::string serialized = original.toJson();
    const ProjectData restored(serialized);

    // Then
    EXPECT_EQ(restored.audio.path, original.audio.path);
    EXPECT_EQ(restored.audio.length, original.audio.length);
    EXPECT_EQ(restored.video.width, original.video.width);
    EXPECT_EQ(restored.video.height, original.video.height);
    EXPECT_EQ(restored.video.background, original.video.background);
    EXPECT_EQ(restored.layout.fontName, original.layout.fontName);
    EXPECT_EQ(restored.layout.fontSize, original.layout.fontSize);
    EXPECT_EQ(restored.layout.lineHeight, original.layout.lineHeight);
    EXPECT_EQ(restored.model.base, original.model.base);
    EXPECT_EQ(restored.rhymeStyles.size(), original.rhymeStyles.size());
    EXPECT_EQ(restored.tokens.size(), original.tokens.size());
    EXPECT_EQ(restored.tokens[0].rhymeIndex.value(), original.tokens[0].rhymeIndex.value());
    EXPECT_FALSE(restored.tokens[1].rhymeIndex.has_value());
    EXPECT_EQ(restored.colorSwatch.size(), original.colorSwatch.size());
}
