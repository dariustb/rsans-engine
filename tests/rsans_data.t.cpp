#include <rsans_data.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using json = nlohmann::json;

TEST(ProjectDataTest, ProjectDataConstructorParsesAudioConfigGivenValidJson) {
    // Given
    const std::string json = R"({
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {
            "path": "test.wav",
            "length": 42
        },
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "colorSwatch": [],
        "tokens": []
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.audio.path, "test.wav");
    EXPECT_EQ(data.audio.length, 42);
}

TEST(ProjectDataTest, ProjectDataConstructorParsesHeaderConfigGivenValidJson) {
    // Given
    const std::string json = R"({
        "header": {
            "fontName": "Lobster",
            "fontPath": "fonts/lobster.ttf",
            "title": "My Song",
            "titleSize": 72,
            "artist": "Some Artist",
            "artistSize": 40,
            "media": "cover.png"
        },
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "colorSwatch": [],
        "tokens": []
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.header.fontName, "Lobster");
    EXPECT_EQ(data.header.fontPath, "fonts/lobster.ttf");
    EXPECT_EQ(data.header.title, "My Song");
    EXPECT_EQ(data.header.titleSize, 72);
    EXPECT_EQ(data.header.artist, "Some Artist");
    EXPECT_EQ(data.header.artistSize, 40);
    EXPECT_EQ(data.header.media, "cover.png");
}

TEST(ProjectDataTest, ProjectDataConstructorParsesVideoConfigGivenValidJson) {
    // Given
    const std::string json = R"({
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {
            "width": 1280,
            "height": 720,
            "background": "#FFFFFF"
        },
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {
            "fontName": "Helvetica",
            "fontPath": "fonts/helvetica.ttf",
            "fontSize": 36,
            "lineHeight": 50
        },
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "colorSwatch": [],
        "tokens": []
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.tokens.size(), 0);
}

TEST(ProjectDataTest, ProjectDataConstructorParsesMixedRhymeGroupsGivenTokens) {
    // Given
    const std::string json = R"({
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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

TEST(ProjectDataTest, ProjectDataMoveConstructorPreservesDataGivenSource) {
    // Given
    const std::string json = R"({
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 42},
        "video": {"width": 1920, "height": 1080, "background": "#123456"},
        "layout": {"fontName": "Times", "fontPath": "fonts/times.ttf", "fontSize": 32, "lineHeight": 40},
        "model": {"path": "models/base.bin"},
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
    EXPECT_EQ(data.header.fontName, "Arial");
    EXPECT_EQ(data.header.title, "Test Title");
    EXPECT_EQ(data.header.titleSize, 64);
    EXPECT_EQ(data.header.artist, "Test Artist");
    EXPECT_EQ(data.header.artistSize, 36);
    EXPECT_EQ(data.header.media, "test.jpg");
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 0},
        "video": {"width": 0, "height": 0, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 0, "lineHeight": 0},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 42},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "music.mp3", "length": 180.5},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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

TEST(ProjectDataTest, ToJsonPreservesHeaderConfigGivenProjectData) {
    // Given
    const std::string inputJson = R"({
        "header": {
            "fontName": "Lobster",
            "fontPath": "fonts/lobster.ttf",
            "title": "My Song",
            "titleSize": 72,
            "artist": "Some Artist",
            "artistSize": 40,
            "media": "cover.png"
        },
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
        "colorSwatch": [],
        "tokens": []
    })";
    const ProjectData data(inputJson);

    // When
    const std::string result = data.toJson();
    const json j = json::parse(result);

    // Then
    EXPECT_EQ(j["header"]["fontName"], "Lobster");
    EXPECT_EQ(j["header"]["fontPath"], "fonts/lobster.ttf");
    EXPECT_EQ(j["header"]["title"], "My Song");
    EXPECT_EQ(j["header"]["titleSize"], 72);
    EXPECT_EQ(j["header"]["artist"], "Some Artist");
    EXPECT_EQ(j["header"]["artistSize"], 40);
    EXPECT_EQ(j["header"]["media"], "cover.png");
}

TEST(ProjectDataTest, ToJsonPreservesVideoConfigGivenProjectData) {
    // Given
    const std::string inputJson = R"({
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1280, "height": 720, "background": "#FFFFFF"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Helvetica", "fontPath": "fonts/helvetica.ttf", "fontSize": 36, "lineHeight": 50},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/whisper-large.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "fontSize": 48, "lineHeight": 60},
        "model": {"path": "models/base.bin"},
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
}

TEST(ProjectDataTest, ToJsonRoundTripPreservesDataGivenCompleteProjectData) {
    // Given
    const std::string inputJson = R"({
        "header": {"fontName": "Arial", "fontPath": "fonts/arial.ttf", "title": "Test Title", "titleSize": 64, "artist": "Test Artist", "artistSize": 36, "media": "test.jpg"},
        "audio": {"path": "song.wav", "length": 240},
        "video": {"width": 3840, "height": 2160, "background": "#123456"},
        "layout": {"fontName": "Roboto", "fontPath": "fonts/roboto.ttf", "fontSize": 64, "lineHeight": 80},
        "model": {"path": "models/large-v3.bin"},
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
    EXPECT_EQ(restored.header.fontName, original.header.fontName);
    EXPECT_EQ(restored.header.fontPath, original.header.fontPath);
    EXPECT_EQ(restored.header.title, original.header.title);
    EXPECT_EQ(restored.header.titleSize, original.header.titleSize);
    EXPECT_EQ(restored.header.artist, original.header.artist);
    EXPECT_EQ(restored.header.artistSize, original.header.artistSize);
    EXPECT_EQ(restored.header.media, original.header.media);
    EXPECT_EQ(restored.audio.path, original.audio.path);
    EXPECT_EQ(restored.audio.length, original.audio.length);
    EXPECT_EQ(restored.video.width, original.video.width);
    EXPECT_EQ(restored.video.height, original.video.height);
    EXPECT_EQ(restored.video.background, original.video.background);
    EXPECT_EQ(restored.layout.fontName, original.layout.fontName);
    EXPECT_EQ(restored.layout.fontSize, original.layout.fontSize);
    EXPECT_EQ(restored.layout.lineHeight, original.layout.lineHeight);
    EXPECT_EQ(restored.model.base, original.model.base);
    EXPECT_EQ(restored.tokens.size(), original.tokens.size());
    EXPECT_EQ(restored.tokens[0].rhymeIndex.value(), original.tokens[0].rhymeIndex.value());
    EXPECT_FALSE(restored.tokens[1].rhymeIndex.has_value());
    EXPECT_EQ(restored.colorSwatch.size(), original.colorSwatch.size());
}

namespace {

bool hasErrorFor(const std::vector<ValidationError>& errors, const std::string& field) {
    return std::any_of(errors.begin(), errors.end(),
                       [&](const ValidationError& e) { return e.field == field; });
}

// Creates empty placeholder files and a project that refers to them, so
// validation (which only checks that files exist) sees a complete project
class ProjectValidationTest : public ::testing::Test {
  protected:
    std::filesystem::path dir;
    ProjectData project;

    std::string touch(const std::string& name) {
        const std::filesystem::path path = dir / name;
        std::ofstream(path).put('x');
        return path.string();
    }

    void SetUp() override {
        dir = std::filesystem::temp_directory_path() / "rsans_validation_test";
        std::filesystem::create_directories(dir);

        project.header.fontName = "Arial";
        project.header.fontPath = touch("font.ttf");
        project.header.title = "Title";
        project.header.titleSize = 64;
        project.header.artist = "Artist";
        project.header.artistSize = 36;
        project.header.media = touch("cover.png");
        project.audio.path = touch("song.wav");
        project.audio.length = 42.0;
        project.video.width = 1920;
        project.video.height = 1080;
        project.video.background = "black";
        project.layout.fontName = "Arial";
        project.layout.fontPath = project.header.fontPath;
        project.layout.fontSize = 48;
        project.layout.lineHeight = 60;
        project.model.base = touch("model.bin");
    }

    void TearDown() override {
        std::filesystem::remove_all(dir);
    }
};

} // namespace

TEST(ProjectDataTest, DefaultConstructorCreatesBlankProject) {
    // When
    const ProjectData project;

    // Then
    EXPECT_TRUE(project.audio.path.empty());
    EXPECT_EQ(project.audio.length, 0.0);
    EXPECT_EQ(project.video.width, 0);
    EXPECT_EQ(project.layout.fontSize, 0);
    EXPECT_TRUE(project.tokens.empty());
    EXPECT_TRUE(project.colorSwatch.empty());
}

TEST(ProjectDataTest, ValidateReportsEveryMandatoryFieldGivenBlankProject) {
    // Given
    const ProjectData project;

    // When
    const std::vector<ValidationError> errors = project.validate();

    // Then
    EXPECT_FALSE(project.isValid());
    for (const std::string field : {
             "header.fontName", "header.fontPath", "header.title", "header.titleSize",
             "header.artist", "header.artistSize",
             "header.media", "audio.path", "audio.length", "video.width", "video.height",
             "video.background", "layout.fontName", "layout.fontPath", "layout.fontSize",
             "layout.lineHeight", "model.path"}) {
        EXPECT_TRUE(hasErrorFor(errors, field)) << "No error for " << field;
    }
}

TEST(ProjectDataTest, ValidateDoesNotRequireOptionalFieldsGivenBlankProject) {
    // Given
    const ProjectData project;

    // When
    const std::vector<ValidationError> errors = project.validate();

    // Then
    for (const std::string field : {"cmudict", "colorSwatch"}) {
        EXPECT_FALSE(hasErrorFor(errors, field)) << "Unexpected error for " << field;
    }
}

TEST_F(ProjectValidationTest, IsValidGivenCompleteProject) {
    EXPECT_TRUE(project.validate().empty());
    EXPECT_TRUE(project.isValid());
}

TEST_F(ProjectValidationTest, ReportsRequiredGivenEmptyTitleAndArtist) {
    // Given
    project.header.title = "";
    project.header.artist = "";

    // When
    const std::vector<ValidationError> errors = project.validate();

    // Then
    ASSERT_EQ(errors.size(), 2);
    EXPECT_TRUE(hasErrorFor(errors, "header.title"));
    EXPECT_TRUE(hasErrorFor(errors, "header.artist"));
}

TEST_F(ProjectValidationTest, ReportsMissingFileGivenNonExistentAudioPath) {
    // Given
    project.audio.path = (dir / "missing.wav").string();

    // When
    const std::vector<ValidationError> errors = project.validate();

    // Then
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].field, "audio.path");
    EXPECT_NE(errors[0].message.find("file not found"), std::string::npos);
}

TEST_F(ProjectValidationTest, ReportsMissingFileGivenDirectoryAsModelPath) {
    // Given
    project.model.base = dir.string();

    // Then
    EXPECT_TRUE(hasErrorFor(project.validate(), "model.path"));
}

TEST_F(ProjectValidationTest, ReportsOddDimensionsGivenOddWidthAndHeight) {
    // Given
    project.video.width = 1921;
    project.video.height = 1081;

    // When
    const std::vector<ValidationError> errors = project.validate();

    // Then
    EXPECT_TRUE(hasErrorFor(errors, "video.width"));
    EXPECT_TRUE(hasErrorFor(errors, "video.height"));
}

TEST_F(ProjectValidationTest, ReportsNegativeAudioLengthGivenUnreadableAudio) {
    // Given: the JSON loader sets -1 when libsndfile cannot read the file
    project.audio.length = -1.0;

    // Then
    EXPECT_TRUE(hasErrorFor(project.validate(), "audio.length"));
}

TEST_F(ProjectValidationTest, IsValidGivenExistingCmudict) {
    // Given
    project.cmudict = touch("cmudict.dict");

    // Then
    EXPECT_TRUE(project.isValid());
}

TEST_F(ProjectValidationTest, ReportsMissingFileGivenNonExistentCmudict) {
    // Given
    project.cmudict = (dir / "missing.dict").string();

    // Then
    EXPECT_TRUE(hasErrorFor(project.validate(), "cmudict"));
}

TEST_F(ProjectValidationTest, ReportsBadColorGivenNonHexSwatchEntry) {
    // Given
    project.colorSwatch = {Color("#FF8800"), Color("#GG0000")};

    // When
    const std::vector<ValidationError> errors = project.validate();

    // Then
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].field, "colorSwatch[1]");
}

TEST_F(ProjectValidationTest, ReportsEmptySwatchGivenTokensWithRhymeGroups) {
    // Given
    project.tokens = {Token(0, "day", 0, 500, 0, 0), Token(1, "way", 500, 1000, 0, 0)};

    // Then
    EXPECT_TRUE(hasErrorFor(project.validate(), "colorSwatch"));
}

TEST_F(ProjectValidationTest, IsValidGivenTokensWithRhymeGroupsAndSwatch) {
    // Given
    project.tokens = {Token(0, "day", 0, 500, 0, 0), Token(1, "way", 500, 1000, 0, 0)};
    project.colorSwatch = {Color("#FF8800")};

    // Then
    EXPECT_TRUE(project.isValid());
}

TEST_F(ProjectValidationTest, ReportsTokenErrorsGivenInvalidToken) {
    // Given
    project.tokens = {Token(0, "", -100, -200, -1, -1)};
    project.colorSwatch = {Color("#FF8800")};

    // When
    const std::vector<ValidationError> errors = project.validate();

    // Then
    EXPECT_TRUE(hasErrorFor(errors, "tokens[0].text"));
    EXPECT_TRUE(hasErrorFor(errors, "tokens[0].startMs"));
    EXPECT_TRUE(hasErrorFor(errors, "tokens[0].endMs"));
    EXPECT_TRUE(hasErrorFor(errors, "tokens[0].lineIndex"));
    EXPECT_TRUE(hasErrorFor(errors, "tokens[0].rhymeIndex"));
}

static_assert(std::is_copy_constructible_v<ProjectData>);
static_assert(std::is_copy_assignable_v<ProjectData>);
static_assert(std::is_nothrow_move_constructible_v<ProjectData>);
static_assert(std::is_nothrow_move_assignable_v<ProjectData>);

TEST_F(ProjectValidationTest, CopyConstructorCopiesEveryMember) {
    // Given
    project.cmudict = "dict";
    project.tokens = {Token(0, "day", 0, 500, 0, 1)};
    project.colorSwatch = {Color("#FF8800")};

    // When
    const ProjectData copy(project);

    // Then
    EXPECT_EQ(copy.toJson(), project.toJson());
}

TEST_F(ProjectValidationTest, CopyIsIndependentOfOriginal) {
    // Given
    project.tokens = {Token(0, "day", 0, 500, 0, std::nullopt)};
    ProjectData copy(project);

    // When
    copy.header.title = "Changed";
    copy.tokens[0].text = "way";

    // Then
    EXPECT_EQ(project.header.title, "Title");
    EXPECT_EQ(project.tokens[0].text, "day");
}

TEST_F(ProjectValidationTest, CopyAssignmentReplacesEveryMember) {
    // Given
    ProjectData target;

    // When
    target = project;

    // Then
    EXPECT_EQ(target.toJson(), project.toJson());
    EXPECT_TRUE(target.isValid());
}

TEST(AudioDurationTest, ReturnsLengthInSecondsGivenWavFile) {
    // Given: a 2 second, 16 kHz mono 16-bit WAV
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "rsans_duration_test.wav";
    const uint32_t sampleRate = 16000;
    const uint32_t dataSize = sampleRate * 2 * 2;
    {
        std::ofstream out(path, std::ios::binary);
        const auto write32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
        const auto write16 = [&](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
        out.write("RIFF", 4);
        write32(36 + dataSize);
        out.write("WAVEfmt ", 8);
        write32(16);
        write16(1);
        write16(1);
        write32(sampleRate);
        write32(sampleRate * 2);
        write16(2);
        write16(16);
        out.write("data", 4);
        write32(dataSize);
        const std::vector<char> silence(dataSize, 0);
        out.write(silence.data(), static_cast<std::streamsize>(silence.size()));
    }

    // When
    const double seconds = getAudioDuration(path.string());

    // Then
    EXPECT_DOUBLE_EQ(seconds, 2.0);
    std::filesystem::remove(path);
}

TEST(AudioDurationTest, ReturnsNegativeOneGivenNonExistentFile) {
    EXPECT_EQ(getAudioDuration("/nonexistent/path/to/audio.wav"), -1.0);
}

// Step-level validation

TEST(StepValidationTest, ValidateForAnalyzeRequiresOnlyAudioAndModelGivenBlankProject) {
    // Given
    const ProjectData project;

    // When
    const std::vector<ValidationError> errors = project.validateForAnalyze();

    // Then
    ASSERT_EQ(errors.size(), 2);
    EXPECT_TRUE(hasErrorFor(errors, "audio.path"));
    EXPECT_TRUE(hasErrorFor(errors, "model.path"));
}

TEST_F(ProjectValidationTest, ValidateForAnalyzeIsReadyGivenOnlyAudioAndModel) {
    // Given: everything else blank
    ProjectData analyzeOnly;
    analyzeOnly.audio.path = project.audio.path;
    analyzeOnly.model.base = project.model.base;

    // Then
    EXPECT_TRUE(analyzeOnly.validateForAnalyze().empty());
    EXPECT_FALSE(analyzeOnly.isValid());
}

TEST_F(ProjectValidationTest, ValidateForRhymeRequiresCmudictAndTokensGivenNeitherSet) {
    // When
    const std::vector<ValidationError> errors = project.validateForRhyme();

    // Then
    ASSERT_EQ(errors.size(), 2);
    EXPECT_TRUE(hasErrorFor(errors, "cmudict"));
    EXPECT_TRUE(hasErrorFor(errors, "tokens"));
}

TEST_F(ProjectValidationTest, ValidateForRhymeIsReadyGivenCmudictAndTokens) {
    // Given
    project.cmudict = touch("cmudict.dict");
    project.tokens = {Token(0, "day", 0, 500, 0, std::nullopt)};

    // Then
    EXPECT_TRUE(project.validateForRhyme().empty());
}

TEST_F(ProjectValidationTest, ValidateForRhymeReportsMissingFileGivenNonExistentCmudict) {
    // Given
    project.cmudict = (dir / "missing.dict").string();
    project.tokens = {Token(0, "day", 0, 500, 0, std::nullopt)};

    // When
    const std::vector<ValidationError> errors = project.validateForRhyme();

    // Then
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].field, "cmudict");
}

TEST_F(ProjectValidationTest, ValidateForExportRequiresTokensGivenNoTokens) {
    // When
    const std::vector<ValidationError> errors = project.validateForExport();

    // Then
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].field, "tokens");
}

TEST_F(ProjectValidationTest, ValidateForExportIsReadyGivenTokensWithoutModelOrCmudict) {
    // Given: export does not read the model or dictionary
    project.model.base = "";
    project.tokens = {Token(0, "day", 0, 500, 0, std::nullopt)};

    // Then
    EXPECT_TRUE(project.validateForExport().empty());
}

TEST_F(ProjectValidationTest, ValidateForExportReportsVisualSettingsGivenBlankHeaderAndLayout) {
    // Given
    project.header = ProjectData::HeaderData{};
    project.layout = ProjectData::LayoutConfig{};
    project.tokens = {Token(0, "day", 0, 500, 0, std::nullopt)};

    // When
    const std::vector<ValidationError> errors = project.validateForExport();

    // Then
    EXPECT_TRUE(hasErrorFor(errors, "header.title"));
    EXPECT_TRUE(hasErrorFor(errors, "header.media"));
    EXPECT_TRUE(hasErrorFor(errors, "layout.fontPath"));
    EXPECT_FALSE(hasErrorFor(errors, "tokens"));
}

TEST_F(ProjectValidationTest, ValidateForExportReportsEmptySwatchGivenRhymedTokens) {
    // Given
    project.tokens = {Token(0, "day", 0, 500, 0, 0)};

    // Then
    EXPECT_TRUE(hasErrorFor(project.validateForExport(), "colorSwatch"));
}

TEST_F(ProjectValidationTest, ValidateForFullIsReadyGivenNoTokens) {
    // Given: tokens come from analyze, so none are needed yet
    project.cmudict = touch("cmudict.dict");
    project.colorSwatch = {Color("#FF8800")};

    // Then
    EXPECT_TRUE(project.validateForFull().empty());
}

TEST_F(ProjectValidationTest, ValidateForFullRequiresCmudictAndSwatchGivenNeitherSet) {
    // When
    const std::vector<ValidationError> errors = project.validateForFull();

    // Then
    ASSERT_EQ(errors.size(), 2);
    EXPECT_TRUE(hasErrorFor(errors, "cmudict"));
    EXPECT_TRUE(hasErrorFor(errors, "colorSwatch"));
}

TEST(StepValidationTest, ValidateForFullReportsEveryInputGivenBlankProject) {
    // Given
    const ProjectData project;

    // When
    const std::vector<ValidationError> errors = project.validateForFull();

    // Then
    for (const std::string field : {
             "header.title", "header.media", "audio.path", "audio.length", "video.width",
             "layout.fontPath", "model.path", "cmudict", "colorSwatch"}) {
        EXPECT_TRUE(hasErrorFor(errors, field)) << "No error for " << field;
    }
    EXPECT_FALSE(hasErrorFor(errors, "tokens"));
}
