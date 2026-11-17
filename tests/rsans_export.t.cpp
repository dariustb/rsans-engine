#include <rsans_export.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

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
