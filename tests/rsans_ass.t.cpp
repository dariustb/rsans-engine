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
    EXPECT_NE(ass.text().find("Style: LyricText"), std::string::npos);
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
    EXPECT_NE(ass.text().find("Style: HeaderTitle"), std::string::npos);
    EXPECT_NE(ass.text().find("Style: HeaderArtist"), std::string::npos);
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
    EXPECT_NE(ass.text().find(",HeaderTitle,"), std::string::npos);
    EXPECT_NE(ass.text().find("Test Artist"), std::string::npos);
    EXPECT_NE(ass.text().find(",HeaderArtist,"), std::string::npos);
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
    EXPECT_NE(ass.text().find(",LyricText,"), std::string::npos);
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
    EXPECT_NE(ass.text().find(",LyricHighlight_0,"), std::string::npos);
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
    EXPECT_NE(ass.text().find("Style: LyricHighlight_0"), std::string::npos);  // highlight style exists
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
    // Pattern matches: outlineWidth=0, shadowDepth=0, alignment=7, marginL=10 in LyricText style
    EXPECT_NE(ass.text().find("Style: LyricText"), std::string::npos);
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
    EXPECT_NE(ass.text().find("Style: LyricHighlight_0"), std::string::npos);
    EXPECT_NE(ass.text().find(",1,0,7,10,"), std::string::npos);
}

TEST(AssTest, AssConstructorIncludesHeaderBkgdStyleGivenHeaderConfig) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then: HeaderBkgd style is present for drawing the background rectangle
    EXPECT_NE(ass.text().find("Style: HeaderBackground"), std::string::npos);
}

TEST(AssTest, AssConstructorIncludesHeaderBackgroundDrawingDialogueGivenHeaderConfig) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then: HeaderBackground layer (2) dialogue uses drawing commands
    EXPECT_NE(ass.text().find("Dialogue: 2,"), std::string::npos);
    EXPECT_NE(ass.text().find("\\p1}m 0 0 l"), std::string::npos);
    EXPECT_NE(ass.text().find("{\\p0}"), std::string::npos);
}

TEST(AssTest, AssConstructorAttachesCommaDirectlyGivenCommaToken) {
    // Given: A comma token should not be preceded by a space in the assembled line
    std::vector<Token> tokens = {
        {1, "Hello", 0, 200, 0, std::nullopt},
        {2, ",",     200, 300, 0, std::nullopt},
        {3, "world", 300, 500, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then: "Hello," with no space before the comma, space before "world"
    EXPECT_NE(ass.text().find("Hello, world"), std::string::npos);
    EXPECT_EQ(ass.text().find("Hello ,"), std::string::npos);
}

TEST(AssTest, AssConstructorUsesTokenStartTimeForHighlightGivenRhymeToken) {
    // Given: Rhyme token starts at 3000ms = 0:00:03.00
    std::vector<Token> tokens = {
        {1, "test", 3000, 5000, 0, 0}
    };
    const ProjectData data = createTestProjectData(tokens, {"#FF0000"});

    // When
    const Ass ass(data);

    // Then: Highlight dialogue starts at token's startMs, not at 0
    EXPECT_NE(ass.text().find("Dialogue: 0,0:00:03.00,"), std::string::npos);
}

TEST(AssTest, AssConstructorGeneratesDistinctStylesGivenMultipleRhymeGroups) {
    // Given: Two tokens with different rhyme groups on different lines
    std::vector<Token> tokens = {
        {1, "cat", 0, 500, 0, 0},
        {2, "dog", 500, 1000, 1, 1}
    };
    const ProjectData data = createTestProjectData(tokens, {"#FF0000", "#00FF00"});

    // When
    const Ass ass(data);

    // Then: Both rhyme styles are defined
    EXPECT_NE(ass.text().find("Style: LyricHighlight_0"), std::string::npos);
    EXPECT_NE(ass.text().find("Style: LyricHighlight_1"), std::string::npos);

    // And both highlight dialogues are generated
    size_t pos = 0;
    int count = 0;
    while ((pos = ass.text().find("Dialogue: 0,", pos)) != std::string::npos) {
        ++count;
        pos += 12;
    }
    EXPECT_EQ(count, 2);
}

TEST(AssTest, AssConstructorCyclesRhymeColorThroughSwatchGivenHighRhymeIndex) {
    // Given: rhymeIndex=2 with 2-color swatch -> maps to rhyme_0 (2 % 2 = 0)
    std::vector<Token> tokens = {
        {1, "word", 0, 500, 0, 2}
    };
    const ProjectData data = createTestProjectData(tokens, {"#FF0000", "#00FF00"});

    // When
    const Ass ass(data);

    // Then: The highlight dialogue references rhyme_0 (not rhyme_2)
    EXPECT_NE(ass.text().find(",LyricHighlight_0,"), std::string::npos);
    EXPECT_EQ(ass.text().find(",LyricHighlight_2,"), std::string::npos);
}

TEST(AssTest, AssConstructorIncludesNoWrapTagGivenTextDialogue) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then: Text dialogues use NoWrap (\q2)
    EXPECT_NE(ass.text().find("\\q2}"), std::string::npos);
}

TEST(AssTest, AssConstructorOutputsSectionsInCorrectOrder) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);
    const std::string& text = ass.text();

    // Then: [Script Info] -> [V4+ Styles] -> [Events]
    const size_t scriptInfoPos = text.find("[Script Info]");
    const size_t stylesPos     = text.find("[V4+ Styles]");
    const size_t eventsPos     = text.find("[Events]");

    ASSERT_NE(scriptInfoPos, std::string::npos);
    ASSERT_NE(stylesPos, std::string::npos);
    ASSERT_NE(eventsPos, std::string::npos);

    EXPECT_LT(scriptInfoPos, stylesPos);
    EXPECT_LT(stylesPos, eventsPos);
}

TEST(AssTest, AssConstructorFormatsEndTimeWithCentisecondsGivenAudioLength) {
    // Given: audio.length = 10.5 -> audioLengthMs = 10500 -> 0:00:10.50
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then: End time includes centiseconds (50 cs for 10500ms)
    EXPECT_NE(ass.text().find("0:00:10.50"), std::string::npos);
}

TEST(AssTest, AssConstructorFormatsTimeWithMinutesGivenLongStartTime) {
    // Given: rhyme token startMs = 75000ms = 1 min 15 sec -> 0:01:15.00
    std::vector<Token> tokens = {
        {1, "word", 75000, 80000, 0, 0}
    };
    const ProjectData data = createTestProjectData(tokens, {"#FF0000"});

    // When
    const Ass ass(data);

    // Then: Highlight dialogue start time has minutes
    EXPECT_NE(ass.text().find("0:01:15.00"), std::string::npos);
}

TEST(AssTest, AssConstructorIncludesBoldFieldInStyleLine) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then: ASS bold field is -1 (true) for the Base style (default isBold = true)
    // Style format: ...backColor,Bold,... -> &H00000000,-1,...
    EXPECT_NE(ass.text().find("&H00000000,-1,"), std::string::npos);
}

TEST(AssTest, AssConstructorIncludesFormatLineInStylesSection) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then: [V4+ Styles] section has the required Format line
    EXPECT_NE(ass.text().find("Format: Name, Fontname, Fontsize,"), std::string::npos);
}

TEST(AssTest, AssConstructorIncludesEncodingFieldInStyleLine) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then: Style line ends with encoding=1
    // Pattern: marginV=10, encoding=1 -> ",10,1\n"
    EXPECT_NE(ass.text().find(",10,1\n"), std::string::npos);
}

TEST(AssTest, AssConstructorHandlesNonConsecutiveLineIndicesGivenGapInLines) {
    // Given: Tokens on lines 0 and 5 (no tokens on lines 1-4)
    std::vector<Token> tokens = {
        {1, "first",  0,   500, 0, std::nullopt},
        {2, "second", 500, 1000, 5, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then: Both lines appear in the output
    EXPECT_NE(ass.text().find("first"),  std::string::npos);
    EXPECT_NE(ass.text().find("second"), std::string::npos);
}

TEST(AssTest, AssConstructorIncludesEventsFormatLineGivenOutput) {
    // Given
    std::vector<Token> tokens = {
        {1, "test", 0, 100, 0, std::nullopt}
    };
    const ProjectData data = createTestProjectData(tokens);

    // When
    const Ass ass(data);

    // Then: [Events] section has the required Format line
    EXPECT_NE(ass.text().find("Format: Layer, Start, End, Style, Name,"), std::string::npos);
}
