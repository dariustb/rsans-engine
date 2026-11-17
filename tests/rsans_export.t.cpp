#include <rsans_export.h>

#include <rsans_data.h>
#include <rsans_process.h>
#include <rsans_task.h>
#include <rsans_test_utils.h>

#include <gtest/gtest.h>

#include <algorithm>
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

bool ffmpegAvailable() {
    try {
        return runProcess({"ffmpeg", "-version"}) == 0;
    } catch (const std::runtime_error&) {
        return false;
    }
}

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
