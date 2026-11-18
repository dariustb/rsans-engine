#include <rsans_export.h>

#include <rsans_data.h>
#include <rsans_process.h>
#include <rsans_task.h>
#include <rsans_test_utils.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>

#include <unistd.h>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

TEST(SubtitlePathTest, SubtitlePathForReplacesExtensionGivenRelativePath) {
    EXPECT_EQ(subtitlePathFor("out.mp4"), "out.ass");
}

TEST(SubtitlePathTest, SubtitlePathForKeepsDirectoryGivenAbsolutePath) {
    EXPECT_EQ(subtitlePathFor("/home/user/songs/my song.mp4"), "/home/user/songs/my song.ass");
}

TEST(SubtitlePathTest, SubtitlePathForReplacesOnlyLastExtensionGivenDottedName) {
    EXPECT_EQ(subtitlePathFor("songs/track.v2.final.mkv"), "songs/track.v2.final.ass");
}

TEST(SubtitlePathTest, SubtitlePathForAppendsExtensionGivenNoExtension) {
    EXPECT_EQ(subtitlePathFor("songs/output"), "songs/output.ass");
}

TEST(SubtitlePathTest, SubtitlePathForThrowsGivenAssOutputPath) {
    EXPECT_THROW(subtitlePathFor("songs/output.ass"), std::invalid_argument);
}

// Export with a real ffmpeg. Skipped where ffmpeg or a test font is missing.

namespace {

const char* const testFont = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

class ExportTest : public ::testing::Test {
  protected:
    std::filesystem::path dir;
    ProjectData project;
    std::string output;

    void SetUp() override {
        if (!ffmpegAvailable()) {
            GTEST_SKIP() << "ffmpeg not found";
        }
        if (!std::filesystem::exists(testFont)) {
            GTEST_SKIP() << "Test font not found: " << testFont;
        }

        dir = std::filesystem::temp_directory_path() / "rsans export test";
        std::filesystem::create_directories(dir);
        const std::string cover = (dir / "cover.png").string();
        ASSERT_EQ(runProcess({"ffmpeg", "-nostdin", "-y", "-loglevel", "error",
                              "-f", "lavfi", "-i", "color=c=red:s=320x60",
                              "-frames:v", "1", cover}), 0);

        project.header = {"DejaVu Sans", testFont, "Title", 24, "Artist", 18, cover};
        // Long and large enough that ffmpeg reports progress (every 0.5 s) before finishing
        project.audio.path = writeTestWav("rsans_export_test.wav", 1, 16000, 300000);
        project.audio.length = 300.0;
        project.video = {1280, 720, "black"};
        project.layout = {"DejaVu Sans", testFont, 20, 26};
        project.tokens = {Token(0, "hello", 0, 1000, 0, 0), Token(1, "world", 1000, 2000, 0, std::nullopt)};
        project.colorSwatch = {Color("#FF8800")};
        output = (dir / "out video.mp4").string();
        ASSERT_TRUE(project.validateForExport().empty());
    }

    void TearDown() override {
        std::filesystem::remove(project.audio.path);
        std::filesystem::remove_all(dir);
    }
};

} // namespace

TEST_F(ExportTest, ExportVideoReportsIncreasingProgressEndingAtOne) {
    // Given
    std::vector<double> fractions;
    const ProgressHandler onProgress = [&](double f) { fractions.push_back(f); };

    // When
    exportVideo(project, output, "ffmpeg", {}, onProgress);

    // Then
    ASSERT_GE(fractions.size(), 2);
    EXPECT_TRUE(std::is_sorted(fractions.begin(), fractions.end()));
    EXPECT_GE(fractions.front(), 0.0);
    EXPECT_LT(fractions.front(), 1.0);
    EXPECT_DOUBLE_EQ(fractions.back(), 1.0);
    EXPECT_TRUE(std::filesystem::exists(output));
    EXPECT_TRUE(std::filesystem::exists(subtitlePathFor(output)));
}

TEST_F(ExportTest, ExportVideoKeepsProgressLinesOutOfLogGivenOutputHandler) {
    // Given: a short render is enough here (the video stops at audio.length)
    project.audio.length = 5.0;
    std::vector<std::string> lines;
    const FfmpegOutputHandler onOutput = [&](const std::string& line) { lines.push_back(line); };

    // When
    exportVideo(project, output, "ffmpeg", onOutput);

    // Then: ffmpeg's log comes through, but not its key=value progress lines
    EXPECT_FALSE(lines.empty());
    for (const std::string& line : lines) {
        EXPECT_EQ(line.rfind("out_time_us=", 0), std::string::npos) << line;
        EXPECT_EQ(line.rfind("progress=", 0), std::string::npos) << line;
    }
}

TEST_F(ExportTest, ExportVideoDeletesPartialFilesGivenCancelDuringRender) {
    // Given: cancel as soon as rendering reports any progress
    CancelFlag cancel;
    double cancelledAt = -1.0;
    const ProgressHandler onProgress = [&](double f) {
        if (!cancel.isRequested()) {
            cancelledAt = f;
            cancel.request();
        }
    };

    // When / Then: stopped partway, not after ffmpeg had already finished
    EXPECT_THROW(exportVideo(project, output, "ffmpeg", {}, onProgress, &cancel), CancelledError);
    EXPECT_GE(cancelledAt, 0.0);
    EXPECT_LT(cancelledAt, 1.0);
    EXPECT_FALSE(std::filesystem::exists(output));
    EXPECT_FALSE(std::filesystem::exists(subtitlePathFor(output)));
}

TEST_F(ExportTest, ExportVideoWritesNothingGivenCancelAlreadyRequested) {
    // Given
    CancelFlag cancel;
    cancel.request();

    // When / Then
    EXPECT_THROW(exportVideo(project, output, "ffmpeg", {}, {}, &cancel), CancelledError);
    EXPECT_FALSE(std::filesystem::exists(output));
    EXPECT_FALSE(std::filesystem::exists(subtitlePathFor(output)));
}

// Fonts that aren't installed, in unrelated folders

namespace {

const char* const testMonoFont = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";

// Hides the system's fonts from ffmpeg for one test (via an empty fontconfig
// setup), so the only fonts libass can find are the ones given by fontsdir
class NoSystemFonts {
  public:
    explicit NoSystemFonts(const std::filesystem::path& dir) {
        std::filesystem::create_directories(dir / "cache");
        std::ofstream(dir / "fonts.conf")
            << "<?xml version=\"1.0\"?>\n<!DOCTYPE fontconfig SYSTEM \"fonts.dtd\">\n"
            << "<fontconfig><cachedir>" << (dir / "cache").string() << "</cachedir></fontconfig>\n";
        if (const char* old = std::getenv("FONTCONFIG_FILE")) {
            d_old = old;
        }
        ::setenv("FONTCONFIG_FILE", (dir / "fonts.conf").c_str(), 1);
    }
    ~NoSystemFonts() {
        d_old ? ::setenv("FONTCONFIG_FILE", d_old->c_str(), 1) : ::unsetenv("FONTCONFIG_FILE");
    }
    NoSystemFonts(const NoSystemFonts&) = delete;
    NoSystemFonts& operator=(const NoSystemFonts&) = delete;
    NoSystemFonts(NoSystemFonts&&) = delete;
    NoSystemFonts& operator=(NoSystemFonts&&) = delete;

  private:
    std::optional<std::string> d_old;
};

// The font libass chose for a family, from its log line
// "fontselect: (DejaVu Sans, 700, 0) -> DejaVuSans, 0, DejaVuSans" (a font
// from the fonts folder is named by its PostScript name; a font it couldn't
// find gets a "failed to find" line instead)
std::string fontChosenFor(const std::vector<std::string>& lines, const std::string& family) {
    const std::string start = "fontselect: (" + family + ",";
    for (const std::string& line : lines) {
        const std::size_t at = line.find(start);
        const std::size_t arrow = line.find(" -> ", at);
        if (at != std::string::npos && arrow != std::string::npos) {
            const std::size_t end = line.find(',', arrow);
            return line.substr(arrow + 4, end - arrow - 4);
        }
    }
    return "";
}

// The files libass loaded from the fonts folder: "Loading font file '<path>'"
std::vector<std::string> loadedFontFiles(const std::vector<std::string>& lines) {
    const std::string start = "Loading font file '";
    std::vector<std::string> files;
    for (const std::string& line : lines) {
        const std::size_t at = line.find(start);
        if (at != std::string::npos) {
            const std::size_t from = at + start.size();
            files.push_back(line.substr(from, line.rfind('\'') - from));
        }
    }
    return files;
}

} // namespace

TEST_F(ExportTest, ExportVideoFindsFontsInUnrelatedFolders) {
    if (!std::filesystem::exists(testMonoFont)) {
        GTEST_SKIP() << "Test font not found: " << testMonoFont;
    }
    // Given: two fonts that aren't installed, in folders that only share dir
    const std::filesystem::path headerFont = dir / "downloads" / "mono" / "Mono.ttf";
    const std::filesystem::path lyricsFont = dir / "my fonts" / "a" / "b" / "Sans.ttf";
    std::filesystem::create_directories(headerFont.parent_path());
    std::filesystem::create_directories(lyricsFont.parent_path());
    std::filesystem::copy_file(testMonoFont, headerFont);
    std::filesystem::copy_file(testFont, lyricsFont);
    project.header.fontName = "DejaVu Sans Mono";
    project.header.fontPath = headerFont.string();
    project.layout.fontPath = lyricsFont.string();
    project.audio.length = 1.0;
    const NoSystemFonts noSystemFonts(dir / "fontconfig");
    std::vector<std::string> lines;

    // When
    exportVideo(project, output, "ffmpeg", [&](const std::string& line) { lines.push_back(line); });

    // Then: each family came from its own font, not a fallback
    std::string log;
    for (const std::string& line : lines) {
        log += line + "\n";
    }
    EXPECT_EQ(fontChosenFor(lines, "DejaVu Sans Mono"), "DejaVuSansMono") << log;
    EXPECT_EQ(fontChosenFor(lines, "DejaVu Sans"), "DejaVuSans") << log;

    // Loaded from copies in one folder of their own, which are gone afterwards,
    // and the originals kept
    const std::vector<std::string> loaded = loadedFontFiles(lines);
    ASSERT_FALSE(loaded.empty()) << log;
    for (const std::string& file : loaded) {
        EXPECT_NE(file.find("rsans-fonts-"), std::string::npos) << file;
        EXPECT_FALSE(std::filesystem::exists(file)) << file;
        EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(file).parent_path())) << file;
    }
    EXPECT_TRUE(std::filesystem::exists(headerFont));
    EXPECT_TRUE(std::filesystem::exists(lyricsFont));
}

// The fonts folder for one export

namespace {

class StagedFontsTest : public ::testing::Test {
  protected:
    std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("rsans_staged_" + std::to_string(::getpid()));
    void SetUp() override {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir / "a");
        std::filesystem::create_directories(dir / "b" / "c");
        std::ofstream(dir / "a" / "Regular.ttf") << "font a";
        std::ofstream(dir / "b" / "c" / "Regular.ttf") << "font b";
    }
    void TearDown() override { std::filesystem::remove_all(dir); }

    static std::string read(const std::string& path) {
        std::ifstream in(path);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }
};

} // namespace

TEST_F(StagedFontsTest, CopiesSameNamedFontsWithoutCollidingAndDeletesThem) {
    std::string stagedDir;
    {
        const StagedFonts fonts({(dir / "a" / "Regular.ttf").string(), (dir / "b" / "c" / "Regular.ttf").string()});
        stagedDir = fonts.dir();

        ASSERT_EQ(fonts.files().size(), 2);
        EXPECT_NE(fonts.files()[0], fonts.files()[1]);
        EXPECT_EQ(read(fonts.files()[0]), "font a");
        EXPECT_EQ(read(fonts.files()[1]), "font b");
        for (const std::string& file : fonts.files()) {
            EXPECT_EQ(std::filesystem::path(file).parent_path(), stagedDir);
        }
        // Only the fonts: nothing else for fontconfig to search
        EXPECT_EQ(std::distance(std::filesystem::directory_iterator(stagedDir),
                                std::filesystem::directory_iterator()), 2);
    }
    EXPECT_FALSE(std::filesystem::exists(stagedDir));
    EXPECT_TRUE(std::filesystem::exists(dir / "a" / "Regular.ttf"));  // Originals kept
}

TEST_F(StagedFontsTest, SameFontTwiceIsCopiedOnce) {
    const std::string font = (dir / "a" / "Regular.ttf").string();
    const std::string sameFont = (dir / "b" / ".." / "a" / "Regular.ttf").string();

    const StagedFonts fonts({font, sameFont});

    ASSERT_EQ(fonts.files().size(), 2);
    EXPECT_EQ(fonts.files()[0], fonts.files()[1]);
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(fonts.dir()),
                            std::filesystem::directory_iterator()), 1);
}

TEST_F(StagedFontsTest, MissingFontThrowsAndLeavesNoFolder) {
    const auto stagedFolders = [] {
        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::temp_directory_path())) {
            count += entry.path().filename().string().rfind("rsans-fonts-", 0) == 0 ? 1 : 0;
        }
        return count;
    };
    const int before = stagedFolders();

    EXPECT_THROW(StagedFonts({(dir / "a" / "Regular.ttf").string(), (dir / "missing.ttf").string()}),
                 std::runtime_error);
    EXPECT_EQ(stagedFolders(), before);
}

TEST_F(ExportTest, ExportVideoLeavesNoFontFolderGivenCancel) {
    // The staged fonts go even when ffmpeg is stopped partway
    CancelFlag cancel;
    std::vector<std::string> lines;
    const FfmpegOutputHandler onOutput = [&](const std::string& line) { lines.push_back(line); };
    const ProgressHandler onProgress = [&](double) { cancel.request(); };

    EXPECT_THROW(exportVideo(project, output, "ffmpeg", onOutput, onProgress, &cancel), CancelledError);

    const std::vector<std::string> loaded = loadedFontFiles(lines);
    ASSERT_FALSE(loaded.empty());
    for (const std::string& file : loaded) {
        EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(file).parent_path())) << file;
    }
}
