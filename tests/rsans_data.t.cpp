#include <rsans_data.h>
#include <gtest/gtest.h>
#include <sstream>

TEST(ProjectDataTest, ProjectDataConstructorParsesAudioConfigGivenValidJson) {
    // Given
    const std::string json = R"({
        "audio": {
            "path": "test.wav",
            "length": 42
        },
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {},
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
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {},
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
            "fontSize": 36,
            "lineHeight": 50
        },
        "rhymeStyles": {},
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
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {},
        "tokens": [
            {
                "id": 1,
                "text": "hello",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": 0,
                "rhymeGroup": null
            },
            {
                "id": 2,
                "text": "world",
                "startMs": 500,
                "endMs": 1000,
                "lineIndex": 0,
                "rhymeGroup": null
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
    EXPECT_FALSE(data.tokens[0].rhymeGroup.has_value());

    EXPECT_EQ(data.tokens[1].id, 2);
    EXPECT_EQ(data.tokens[1].text, "world");
    EXPECT_EQ(data.tokens[1].startMs, 500);
    EXPECT_EQ(data.tokens[1].endMs, 1000);
    EXPECT_EQ(data.tokens[1].lineIndex, 0);
    EXPECT_FALSE(data.tokens[1].rhymeGroup.has_value());
}

TEST(ProjectDataTest, ProjectDataConstructorParsesRhymeGroupGivenTokenWithRhyme) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {"A": {"color": "#FF0000"}},
        "tokens": [
            {
                "id": 1,
                "text": "cat",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": 0,
                "rhymeGroup": "A"
            }
        ]
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.tokens.size(), 1);
    EXPECT_TRUE(data.tokens[0].rhymeGroup.has_value());
    EXPECT_EQ(data.tokens[0].rhymeGroup.value(), "A");
}

TEST(ProjectDataTest, ProjectDataConstructorParsesRhymeStylesGivenValidJson) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {
            "A": {"color": "#FF0000"},
            "B": {"color": "#00FF00"}
        },
        "tokens": []
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.rhymeStyles.size(), 2);
    EXPECT_EQ(data.rhymeStyles.at("A").color, "#FF0000");
    EXPECT_EQ(data.rhymeStyles.at("B").color, "#00FF00");
}

TEST(ProjectDataTest, ProjectDataConstructorHandlesEmptyTokensGivenEmptyArray) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {},
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
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {},
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
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {"A": {"color": "#FF0000"}},
        "tokens": [
            {
                "id": 1,
                "text": "cat",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": 0,
                "rhymeGroup": "A"
            },
            {
                "id": 2,
                "text": "and",
                "startMs": 500,
                "endMs": 700,
                "lineIndex": 0,
                "rhymeGroup": null
            },
            {
                "id": 3,
                "text": "hat",
                "startMs": 700,
                "endMs": 1000,
                "lineIndex": 0,
                "rhymeGroup": "A"
            }
        ]
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.tokens.size(), 3);
    EXPECT_TRUE(data.tokens[0].rhymeGroup.has_value());
    EXPECT_EQ(data.tokens[0].rhymeGroup.value(), "A");
    EXPECT_FALSE(data.tokens[1].rhymeGroup.has_value());
    EXPECT_TRUE(data.tokens[2].rhymeGroup.has_value());
    EXPECT_EQ(data.tokens[2].rhymeGroup.value(), "A");
}

TEST(ProjectDataTest, ProjectDataConstructorParsesMultipleLinesGivenDifferentLineIndices) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {},
        "tokens": [
            {
                "id": 1,
                "text": "line",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": 0,
                "rhymeGroup": null
            },
            {
                "id": 2,
                "text": "one",
                "startMs": 500,
                "endMs": 1000,
                "lineIndex": 1,
                "rhymeGroup": null
            },
            {
                "id": 3,
                "text": "two",
                "startMs": 1000,
                "endMs": 1500,
                "lineIndex": 2,
                "rhymeGroup": null
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
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {"A": {"color": "#FF0000"}},
        "tokens": [
            {
                "id": 1,
                "text": "old",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": 0,
                "rhymeGroup": null
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
    EXPECT_EQ(data.rhymeStyles.size(), 1);
}

TEST(ProjectDataTest, ProjectDataMoveConstructorPreservesDataGivenSource) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 42},
        "video": {"width": 1920, "height": 1080, "background": "#123456"},
        "layout": {"fontName": "Times", "fontSize": 32, "lineHeight": 40},
        "rhymeStyles": {"X": {"color": "#ABCDEF"}},
        "tokens": [
            {
                "id": 99,
                "text": "moved",
                "startMs": 10,
                "endMs": 20,
                "lineIndex": 5,
                "rhymeGroup": "X"
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
    EXPECT_EQ(data.rhymeStyles.size(), 1);
    EXPECT_EQ(data.rhymeStyles.at("X").color, "#ABCDEF");
    EXPECT_EQ(data.tokens.size(), 1);
    EXPECT_EQ(data.tokens[0].id, 99);
    EXPECT_EQ(data.tokens[0].text, "moved");
}

TEST(ProjectDataTest, ProjectDataConstructorParsesLargeTokenArrayGivenManyTokens) {
    // Given
    std::ostringstream json;
    json << R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {},
        "tokens": [)";

    for (int i = 0; i < 100; ++i) {
        if (i > 0) json << ",";
        json << R"({
            "id": )" << i << R"(,
            "text": "token)" << i << R"(",
            "startMs": )" << (i * 100) << R"(,
            "endMs": )" << ((i + 1) * 100) << R"(,
            "lineIndex": 0,
            "rhymeGroup": null
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
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {},
        "tokens": [
            {
                "id": 1,
                "text": "café",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": 0,
                "rhymeGroup": null
            },
            {
                "id": 2,
                "text": "日本語",
                "startMs": 500,
                "endMs": 1000,
                "lineIndex": 0,
                "rhymeGroup": null
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
        "layout": {"fontName": "Arial", "fontSize": 0, "lineHeight": 0},
        "rhymeStyles": {},
        "tokens": [
            {
                "id": 0,
                "text": "zero",
                "startMs": 0,
                "endMs": 0,
                "lineIndex": 0,
                "rhymeGroup": null
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
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {},
        "tokens": [
            {
                "id": 1,
                "text": "negative",
                "startMs": 0,
                "endMs": 500,
                "lineIndex": -1,
                "rhymeGroup": null
            }
        ]
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.tokens[0].lineIndex, -1);
}

TEST(ProjectDataTest, ProjectDataConstructorParsesMultipleRhymeStylesGivenComplexJson) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {
            "A": {"color": "#FF0000"},
            "B": {"color": "#00FF00"},
            "C": {"color": "#0000FF"},
            "Group1": {"color": "#FFFF00"}
        },
        "tokens": []
    })";

    // When
    const ProjectData data(json);

    // Then
    EXPECT_EQ(data.rhymeStyles.size(), 4);
    EXPECT_EQ(data.rhymeStyles.at("A").color, "#FF0000");
    EXPECT_EQ(data.rhymeStyles.at("B").color, "#00FF00");
    EXPECT_EQ(data.rhymeStyles.at("C").color, "#0000FF");
    EXPECT_EQ(data.rhymeStyles.at("Group1").color, "#FFFF00");
}

TEST(ProjectDataTest, ProjectDataConstructorPreservesTokenOrderGivenSequentialTokens) {
    // Given
    const std::string json = R"({
        "audio": {"path": "test.wav", "length": 10},
        "video": {"width": 1920, "height": 1080, "background": "#000000"},
        "layout": {"fontName": "Arial", "fontSize": 48, "lineHeight": 60},
        "rhymeStyles": {},
        "tokens": [
            {"id": 3, "text": "third", "startMs": 0, "endMs": 100, "lineIndex": 0, "rhymeGroup": null},
            {"id": 1, "text": "first", "startMs": 100, "endMs": 200, "lineIndex": 0, "rhymeGroup": null},
            {"id": 2, "text": "second", "startMs": 200, "endMs": 300, "lineIndex": 0, "rhymeGroup": null}
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
