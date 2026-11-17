#include <rsans_fonts.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace {

const char* const dejaVuSans = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

const std::vector<FontInfo> sampleFonts = {
    {"DejaVu Sans", "Book", "/fonts/DejaVuSans.ttf"},
    {"DejaVu Sans Mono", "Book", "/fonts/DejaVuSansMono.ttf"},
    {"Liberation Mono", "Regular", "/fonts/LiberationMono.ttf"},
    {"Noto Serif", "Regular", "/fonts/NotoSerif.ttf"},
};

std::vector<std::string> families(const std::vector<std::size_t>& indexes) {
    std::vector<std::string> result;
    for (const std::size_t i : indexes) {
        result.push_back(sampleFonts[i].family);
    }
    return result;
}

} // namespace

TEST(FilterFontsTest, FilterFontsReturnsAllGivenEmptyQuery) {
    EXPECT_EQ(filterFonts(sampleFonts, "").size(), sampleFonts.size());
    EXPECT_EQ(filterFonts(sampleFonts, "   ").size(), sampleFonts.size());
}

TEST(FilterFontsTest, FilterFontsIgnoresCaseGivenLowercaseQuery) {
    EXPECT_EQ(families(filterFonts(sampleFonts, "noto")), std::vector<std::string>{"Noto Serif"});
}

TEST(FilterFontsTest, FilterFontsMatchesEveryWordGivenSeveralWords) {
    EXPECT_EQ(families(filterFonts(sampleFonts, "sans mono")),
              std::vector<std::string>{"DejaVu Sans Mono"});
}

TEST(FilterFontsTest, FilterFontsPutsPrefixMatchesFirstGivenQuery) {
    // Neither family starts with "mono", so both keep their original order
    EXPECT_EQ(families(filterFonts(sampleFonts, "mono")),
              (std::vector<std::string>{"DejaVu Sans Mono", "Liberation Mono"}));
    EXPECT_EQ(families(filterFonts(sampleFonts, "s")).front(), "DejaVu Sans");

    const std::vector<FontInfo> fonts = {{"Sans Extra", "", ""}, {"Open Sans", "", ""}, {"Sansita", "", ""}};
    const std::vector<std::size_t> matches = filterFonts(fonts, "sans");
    ASSERT_EQ(matches.size(), 3u);
    EXPECT_EQ(fonts[matches[0]].family, "Sans Extra");
    EXPECT_EQ(fonts[matches[1]].family, "Sansita");
    EXPECT_EQ(fonts[matches[2]].family, "Open Sans");
}

TEST(FilterFontsTest, FilterFontsReturnsNothingGivenNoMatch) {
    EXPECT_TRUE(filterFonts(sampleFonts, "comic").empty());
}

TEST(ScanSystemFontsTest, ScanSystemFontsListsEachFamilyOnceSortedWithExistingFiles) {
    // When
    const std::vector<FontInfo> fonts = scanSystemFonts();
    if (fonts.empty()) {
        GTEST_SKIP() << "No fonts installed";
    }

    // Then
    std::set<std::string> seen;
    for (std::size_t i = 0; i < fonts.size(); ++i) {
        EXPECT_FALSE(fonts[i].family.empty());
        EXPECT_TRUE(std::filesystem::is_regular_file(fonts[i].path)) << fonts[i].path;
        EXPECT_TRUE(seen.insert(lower(fonts[i].family)).second) << "Duplicate " << fonts[i].family;
        if (i > 0) {
            EXPECT_LE(lower(fonts[i - 1].family), lower(fonts[i].family));
        }
    }
}

TEST(ScanSystemFontsTest, ScanSystemFontsUsesRegularFaceGivenFamilyWithBoldAndItalic) {
    // Given: DejaVu Sans is installed with Bold, Oblique, and Bold Oblique faces too
    const std::vector<FontInfo> fonts = scanSystemFonts();
    const auto dejaVu = std::find_if(fonts.begin(), fonts.end(),
                                     [](const FontInfo& f) { return f.family == "DejaVu Sans"; });
    if (dejaVu == fonts.end()) {
        GTEST_SKIP() << "DejaVu Sans not installed";
    }

    // Then
    EXPECT_EQ(std::filesystem::path(dejaVu->path).filename(), "DejaVuSans.ttf");
}

TEST(FontFromFileTest, FontFromFileReadsFamilyGivenFontFile) {
    if (!std::filesystem::exists(dejaVuSans)) {
        GTEST_SKIP() << "DejaVu Sans not installed";
    }

    const std::optional<FontInfo> font = fontFromFile(dejaVuSans);

    ASSERT_TRUE(font.has_value());
    EXPECT_EQ(font->family, "DejaVu Sans");
    EXPECT_EQ(font->path, dejaVuSans);
}

TEST(FontFromFileTest, FontFromFileReturnsNothingGivenNonFontFile) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "rsans_not_a_font.ttf";
    std::ofstream(path) << "not a font";

    EXPECT_FALSE(fontFromFile(path.string()).has_value());
    EXPECT_FALSE(fontFromFile("/nonexistent/font.ttf").has_value());
    std::filesystem::remove(path);
}
