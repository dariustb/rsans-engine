#include <rsans_ass.h>

#include <rsans_data.h>

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <vector>

namespace {

std::string getTestImagePath() {
    static std::string path;
    if (!path.empty()) return path;

    path = "/tmp/test_rsans_1x1.bmp";
    const unsigned char bmp[] = {
        0x42,0x4D,0x3A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x36,0x00,0x00,0x00,
        0x28,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,
        0x18,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0xFF,0xFF,0xFF,0x00
    };
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bmp), sizeof(bmp));
    return path;
}

} // namespace

// Helper function to create a basic ProjectData for testing
ProjectData createTestProjectData(
    const std::vector<Token>& tokens,
    const std::vector<std::string>& colorHexValues = {})
{
    const std::string imagePath = getTestImagePath();

    // Create a minimal JSON with required fields
    std::ostringstream json;
    json << R"({
        "header": {
            "fontName": "DejaVu Sans Mono",
            "fontPath": "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "title": "Test Title",
            "titleSize": 64,
            "artist": "Test Artist",
            "artistSize": 36,
            "media": ")" << imagePath << R"("
        },
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
        "colorSwatch": [)";

    for (size_t i = 0; i < colorHexValues.size(); ++i) {
        if (i > 0) json << ",";
        json << "\"" << colorHexValues[i] << "\"";
    }

    json << R"(],
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
    ASSERT_FALSE(ass.text().empty());
    EXPECT_NE(ass.text().find("[Script Info]"), std::string::npos);
    EXPECT_NE(ass.text().find("[V4+ Styles]"), std::string::npos);
    EXPECT_NE(ass.text().find("[Events]"), std::string::npos);
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
    EXPECT_NE(ass.text().find("PlayResX: 1920"), std::string::npos);
    EXPECT_NE(ass.text().find("PlayResY: 1080"), std::string::npos);
    EXPECT_NE(ass.text().find("ScriptType: v4.00+"), std::string::npos);
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
    EXPECT_NE(ass.text().find("Style: Base"), std::string::npos);
    EXPECT_NE(ass.text().find("DejaVu Sans Mono"), std::string::npos);
}

TEST(AssTest, AssConstructorGeneratesTitleAndArtistStylesGivenHeaderConfig) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text().find("Style: Title"), std::string::npos);
    EXPECT_NE(ass.text().find("Style: Artist"), std::string::npos);
}

TEST(AssTest, AssConstructorGeneratesHeaderLabelDialogueGivenHeaderConfig) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text().find("Dialogue: 3,"), std::string::npos);
    EXPECT_NE(ass.text().find("Test Title"), std::string::npos);
    EXPECT_NE(ass.text().find(",Title,"), std::string::npos);
    EXPECT_NE(ass.text().find("Test Artist"), std::string::npos);
    EXPECT_NE(ass.text().find(",Artist,"), std::string::npos);
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
    EXPECT_NE(ass.text().find("Dialogue: 1,"), std::string::npos);
    EXPECT_NE(ass.text().find("Hello world"), std::string::npos);
    EXPECT_NE(ass.text().find(",Base,"), std::string::npos);
}

TEST(AssTest, AssConstructorGeneratesRhymeDialoguesGivenRhymeTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, 0},
        {2, "hat", 500, 1000, 1, 0}
    };
    const ProjectData data = createTestProjectData(tokens, {"#FF0000"});

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text().find("Dialogue: 0,"), std::string::npos);
    EXPECT_NE(ass.text().find(",rhyme_0,"), std::string::npos);
}

TEST(AssTest, AssConstructorCreatesValidStructureGivenEmptyTokens) {
    // Given
    std::vector<Token> tokens;
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then
    ASSERT_FALSE(ass.text().empty());
    EXPECT_NE(ass.text().find("[Script Info]"), std::string::npos);
    EXPECT_NE(ass.text().find("[V4+ Styles]"), std::string::npos);
    EXPECT_NE(ass.text().find("[Events]"), std::string::npos);
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
    EXPECT_NE(ass.text().find("Solo"), std::string::npos);
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
    EXPECT_NE(ass.text().find("Dialogue: 1,"), std::string::npos);
    EXPECT_NE(ass.text().find("Hello world"), std::string::npos);
    EXPECT_NE(ass.text().find("no rhymes"), std::string::npos);

    size_t pos = 0;
    int count = 0;
    while ((pos = ass.text().find("Dialogue: 0,", pos)) != std::string::npos) {
        ++count;
        pos += 12;
    }
    EXPECT_EQ(count, 0);
}

TEST(AssTest, AssConstructorHandlesMixedTokensGivenBothRhymeAndNonRhyme) {
    // Given
    std::vector<Token> tokens = {
        {1, "The", 0, 200, 0, std::nullopt},
        {2, "cat", 200, 500, 0, 0},
        {3, "sat", 500, 800, 0, std::nullopt},
        {4, "on", 800, 1000, 1, std::nullopt},
        {5, "the", 1000, 1200, 1, std::nullopt},
        {6, "mat", 1200, 1500, 1, 0}
    };
    const ProjectData data = createTestProjectData(tokens, {"#FF0000"});

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text().find("The cat sat"), std::string::npos);
    EXPECT_NE(ass.text().find("on the mat"), std::string::npos);

    size_t pos = 0;
    int count = 0;
    while ((pos = ass.text().find("Dialogue: 0,", pos)) != std::string::npos) {
        ++count;
        pos += 12;
    }
    EXPECT_EQ(count, 2);
}

TEST(AssTest, AssConstructorHandlesDuplicateWordsGivenRepeatedTokensOnSameLine) {
    // Given: Duplicate word "love" appearing twice on the same line
    std::vector<Token> tokens = {
        {1, "love", 0, 500, 0, 0},
        {2, "love", 500, 1000, 0, 0}
    };
    const ProjectData data = createTestProjectData(tokens, {"#FF0000"});

    // When
    const Ass ass(data);

    // Then: Both rhyme dialogues are generated at different X positions
    size_t pos = 0;
    int count = 0;
    while ((pos = ass.text().find("Dialogue: 0,", pos)) != std::string::npos) {
        ++count;
        pos += 12;
    }
    EXPECT_EQ(count, 2);
}

TEST(AssTest, AssConstructorFormatsTimesCorrectlyGivenTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, 0}
    };
    const ProjectData data = createTestProjectData(tokens, {"#FF0000"});

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text().find("0:00:00.00"), std::string::npos);
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
    EXPECT_NE(ass.text().find("Dialogue: 1,0:00:00.00,"), std::string::npos);

    const bool hasValidTime = (ass.text().find("0:00:10") != std::string::npos);
    EXPECT_TRUE(hasValidTime);
}

TEST(AssTest, AssConstructorIncludesPositioningTagsGivenTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, 0}
    };
    const ProjectData data = createTestProjectData(tokens, {"#FF0000"});

    // When
    const Ass ass(data);

    // Then
    EXPECT_NE(ass.text().find("\\pos("), std::string::npos);
}

TEST(AssTest, AssConstructorIncludesHighlightFormattingGivenRhymeTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, 0}
    };
    const ProjectData data = createTestProjectData(tokens, {"#FF0000"});

    // When
    const Ass ass(data);

    // Then: Highlight formatting is defined in the Style (not dialogue override tags)
    EXPECT_NE(ass.text().find("Style: rhyme_0"), std::string::npos);  // highlight style exists
    EXPECT_NE(ass.text().find("&H000000FF"), std::string::npos);       // swatch color #FF0000 in ASS BGR format
    EXPECT_NE(ass.text().find(",3,"), std::string::npos);              // OpaqueBox borderStyle in Style line
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
    EXPECT_NE(ass.text().find("Line one"), std::string::npos);
    EXPECT_NE(ass.text().find("Line two"), std::string::npos);
    EXPECT_NE(ass.text().find("Line three"), std::string::npos);
}

TEST(AssTest, AssConstructorIncludesAlignmentTagsGivenTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then: Alignment is encoded in the Style line (TopLeft=7) not as a dialogue override tag
    // Pattern matches: outlineWidth=0, shadowDepth=0, alignment=7, marginL=10 in Base style
    EXPECT_NE(ass.text().find("Style: Base"), std::string::npos);
    EXPECT_NE(ass.text().find(",0,0,7,10,"), std::string::npos);
}

TEST(AssTest, AssConstructorUsesCorrectAlignmentGivenRhymeTokens) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, 0}
    };
    const ProjectData data = createTestProjectData(tokens, {"#FF0000"});

    // When
    const Ass ass(data);

    // Then: Highlight style also uses TopLeft alignment (7)
    // Pattern matches: outlineWidth=1, shadowDepth=0, alignment=7, marginL=10 in rhyme style
    EXPECT_NE(ass.text().find("Style: rhyme_0"), std::string::npos);
    EXPECT_NE(ass.text().find(",1,0,7,10,"), std::string::npos);
}
