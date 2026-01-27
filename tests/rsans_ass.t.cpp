#include <rsans_ass.h>

#include <rsans_data.h>

#include <gtest/gtest.h>

#include <sstream>
#include <vector>

// Helper function to create a basic ProjectData for testing
ProjectData createTestProjectData(
    const std::vector<Token>& tokens,
    const std::map<std::string, ProjectData::RhymeStyle>& rhymeStyles = {})
{
    // Create a minimal JSON with required fields
    std::ostringstream json;
    json << R"({
        "audio": {
            "path": "test.wav",
            "length": 10.5
        },
        "video": {
            "width": 1920,
            "height": 1080,
            "background": "#000000"
        },
        "layout": {
            "fontName": "DejaVu Sans Mono",
            "fontPath": "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "fontSize": 48,
            "lineHeight": 60
        },
        "model": {
            "path": "models/base.bin"
        },
        "rhymeStyles": {)";

    bool first = true;
    for (const auto& [group, style] : rhymeStyles) {
        if (!first) json << ",";
        json << "\"" << group << "\": {\"color\": \"" << style.color << "\"}";
        first = false;
    }

    json << R"(},
        "tokens": []
    })";

    ProjectData data(json.str());
    return ProjectData(std::move(data), const_cast<std::vector<Token>&>(tokens));
}

TEST(AssTest, AssConstructorCreatesValidOutputGivenBasicTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "Hello", 0, 500, 0, std::nullopt},
        {2, "world", 500, 1000, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    ASSERT_FALSE(ass.text.empty());
    EXPECT_NE(ass.text.find("[Script Info]"), std::string::npos);
    EXPECT_NE(ass.text.find("[V4+ Styles]"), std::string::npos);
    EXPECT_NE(ass.text.find("[Events]"), std::string::npos);
}

TEST(AssTest, AssConstructorGeneratesScriptInfoGivenVideoConfig) {
    // Given: Token with video configuration (1920x1080)
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("PlayResX: 1920"), std::string::npos);
    EXPECT_NE(ass.text.find("PlayResY: 1080"), std::string::npos);
    EXPECT_NE(ass.text.find("ScriptType: v4.00+"), std::string::npos);
}

TEST(AssTest, AssConstructorGeneratesBaseStyleGivenLayoutConfig) {
    // Given: Token with layout configuration
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("Style: Base"), std::string::npos);
    EXPECT_NE(ass.text.find("DejaVu Sans Mono"), std::string::npos);
}

TEST(AssTest, AssConstructorGeneratesRhymeStylesGivenRhymeGroups) {
    // Given: Tokens with rhyme group and corresponding rhyme style
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, "rhyme_0"},
        {2, "hat", 500, 1000, 1, "rhyme_0"}
    };
    std::map<std::string, ProjectData::RhymeStyle> rhymeStyles = {
        {"rhyme_0", {"#FF0000"}}
    };
    const ProjectData data = createTestProjectData(tokens, rhymeStyles);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("Style: rhyme_0"), std::string::npos);
}

TEST(AssTest, AssConstructorGeneratesMultipleRhymeStylesGivenMultipleGroups) {
    // Given: Tokens with two different rhyme groups
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, "rhyme_0"},
        {2, "dog", 500, 1000, 1, "rhyme_1"},
        {3, "hat", 1000, 1500, 2, "rhyme_0"}
    };
    std::map<std::string, ProjectData::RhymeStyle> rhymeStyles = {
        {"rhyme_0", {"#FF0000"}},
        {"rhyme_1", {"#00FF00"}}
    };
    const ProjectData data = createTestProjectData(tokens, rhymeStyles);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("Style: rhyme_0"), std::string::npos);
    EXPECT_NE(ass.text.find("Style: rhyme_1"), std::string::npos);
}

TEST(AssTest, AssConstructorGeneratesBaseDialogueGivenTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "Hello", 0, 500, 0, std::nullopt},
        {2, "world", 500, 1000, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("Dialogue: 1,"), std::string::npos);
    EXPECT_NE(ass.text.find("Hello world"), std::string::npos);
    EXPECT_NE(ass.text.find(",Base,"), std::string::npos);
}

TEST(AssTest, AssConstructorUsesLineSeparatorGivenMultipleLines) {
    // Given
    std::vector<Token> tokens = {
        {1, "First", 0, 500, 0, std::nullopt},
        {2, "line", 500, 1000, 0, std::nullopt},
        {3, "Second", 1000, 1500, 1, std::nullopt},
        {4, "line", 1500, 2000, 1, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("First line\\NSecond line"), std::string::npos);
}

TEST(AssTest, AssConstructorGeneratesRhymeDialoguesGivenRhymeTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, "rhyme_0"},
        {2, "hat", 500, 1000, 1, "rhyme_0"}
    };
    std::map<std::string, ProjectData::RhymeStyle> rhymeStyles = {
        {"rhyme_0", {"#FF0000"}}
    };
    const ProjectData data = createTestProjectData(tokens, rhymeStyles);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("Dialogue: 0,"), std::string::npos);
    EXPECT_NE(ass.text.find(",rhyme_0,"), std::string::npos);
}

TEST(AssTest, AssConstructorCreatesValidStructureGivenEmptyTokens) {
    // Given
    std::vector<Token> tokens;
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    ASSERT_FALSE(ass.text.empty());
    EXPECT_NE(ass.text.find("[Script Info]"), std::string::npos);
    EXPECT_NE(ass.text.find("[V4+ Styles]"), std::string::npos);
    EXPECT_NE(ass.text.find("[Events]"), std::string::npos);
}

TEST(AssTest, AssConstructorCreatesValidOutputGivenSingleToken) {
    // Given
    std::vector<Token> tokens = {
        {1, "Solo", 0, 500, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("Solo"), std::string::npos);
}

TEST(AssTest, AssConstructorOmitsRhymeDialoguesGivenTokensWithoutRhymes) {
    // Given
    std::vector<Token> tokens = {
        {1, "Hello", 0, 500, 0, std::nullopt},
        {2, "world", 500, 1000, 0, std::nullopt},
        {3, "no", 1000, 1500, 1, std::nullopt},
        {4, "rhymes", 1500, 2000, 1, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("Dialogue: 1,"), std::string::npos);
    EXPECT_NE(ass.text.find("Hello world\\Nno rhymes"), std::string::npos);

    size_t pos = 0;
    int count = 0;
    while ((pos = ass.text.find("Dialogue: 0,", pos)) != std::string::npos) {
        ++count;
        pos += 12;
    }
    EXPECT_EQ(count, 0);
}

TEST(AssTest, AssConstructorHandlesMixedTokensGivenBothRhymeAndNonRhyme) {
    // Given
    std::vector<Token> tokens = {
        {1, "The", 0, 200, 0, std::nullopt},
        {2, "cat", 200, 500, 0, "A"},
        {3, "sat", 500, 800, 0, std::nullopt},
        {4, "on", 800, 1000, 1, std::nullopt},
        {5, "the", 1000, 1200, 1, std::nullopt},
        {6, "mat", 1200, 1500, 1, "A"}
    };
    std::map<std::string, ProjectData::RhymeStyle> rhymeStyles = {
        {"A", {"#FF0000"}}
    };
    const ProjectData data = createTestProjectData(tokens, rhymeStyles);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("The cat sat\\Non the mat"), std::string::npos);

    size_t pos = 0;
    int count = 0;
    while ((pos = ass.text.find("Dialogue: 0,", pos)) != std::string::npos) {
        ++count;
        pos += 12;
    }
    EXPECT_EQ(count, 2);
}

TEST(AssTest, AssConstructorHandlesDuplicateWordsGivenRepeatedTokensOnSameLine) {
    // Given: Duplicate word "love" appearing twice on the same line
    std::vector<Token> tokens = {
        {1, "love", 0, 500, 0, "A"},
        {2, "love", 500, 1000, 0, "A"}
    };
    std::map<std::string, ProjectData::RhymeStyle> rhymeStyles = {
        {"A", {"#FF0000"}}
    };
    const ProjectData data = createTestProjectData(tokens, rhymeStyles);

    // When
    const Ass ass(data);

    // Then: Both rhyme dialogues are generated
    // Note: This test exposes a potential bug where both dialogues
    // may be positioned at the same X coordinate
    size_t pos = 0;
    int count = 0;
    while ((pos = ass.text.find("Dialogue: 0,", pos)) != std::string::npos) {
        ++count;
        pos += 12;
    }
    EXPECT_EQ(count, 2);
}

TEST(AssTest, AssConstructorFormatsTimesCorrectlyGivenTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, "A"}
    };
    std::map<std::string, ProjectData::RhymeStyle> rhymeStyles = {
        {"A", {"#FF0000"}}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("0:00:00.00"), std::string::npos);
}

TEST(AssTest, AssConstructorUsesAudioLengthWhenCalculatingEndTime) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("Dialogue: 1,0:00:00.00,"), std::string::npos);

    const bool hasValidTime = (ass.text.find("0:00:10") != std::string::npos);
    EXPECT_TRUE(hasValidTime);
}

TEST(AssTest, AssConstructorIncludesPositioningTagsGivenTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, "A"}
    };
    std::map<std::string, ProjectData::RhymeStyle> rhymeStyles = {
        {"A", {"#FF0000"}}
    };
    const ProjectData data = createTestProjectData(tokens, rhymeStyles);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("\\pos("), std::string::npos);
}

TEST(AssTest, AssConstructorIncludesDrawingCommandsGivenRhymeTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, "A"}
    };
    std::map<std::string, ProjectData::RhymeStyle> rhymeStyles = {
        {"A", {"#FF0000"}}
    };
    const ProjectData data = createTestProjectData(tokens, rhymeStyles);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("\\p1}"), std::string::npos);  // Start drawing
    EXPECT_NE(ass.text.find("{\\p0}"), std::string::npos);  // End drawing
    EXPECT_NE(ass.text.find("m 0 0 l"), std::string::npos);  // Drawing commands
}

TEST(AssTest, AssConstructorPositionsLinesVerticallyGivenMultipleLines) {
    // Given
    std::vector<Token> tokens = {
        {1, "Line", 0, 500, 0, std::nullopt},
        {2, "one", 500, 1000, 0, std::nullopt},
        {3, "Line", 1000, 1500, 1, std::nullopt},
        {4, "two", 1500, 2000, 1, std::nullopt},
        {5, "Line", 2000, 2500, 2, std::nullopt},
        {6, "three", 2500, 3000, 2, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("Line one\\NLine two\\NLine three"), std::string::npos);
}

TEST(AssTest, AssConstructorConvertsColorsToAssFormatGivenRhymeStyles) {
    // Given
    std::vector<Token> tokens = {
        {1, "red", 0, 500, 0, "rhyme_0"}
    };
    std::map<std::string, ProjectData::RhymeStyle> rhymeStyles = {
        {"rhyme_0", {"#FF0000"}}
    };
    const ProjectData data = createTestProjectData(tokens, rhymeStyles);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("Style: rhyme_0"), std::string::npos);
    EXPECT_NE(ass.text.find("&H00"), std::string::npos);
}

TEST(AssTest, AssConstructorIncludesAlignmentTagsGivenTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("\\an7"), std::string::npos);
}

TEST(AssTest, AssConstructorUsesCorrectAlignmentGivenRhymeTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, "A"}
    };
    std::map<std::string, ProjectData::RhymeStyle> rhymeStyles = {
        {"A", {"#FF0000"}}
    };
    const ProjectData data = createTestProjectData(tokens, rhymeStyles);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text.find("\\an7"), std::string::npos);
}
