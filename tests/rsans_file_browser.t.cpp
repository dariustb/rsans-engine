#include <rsans_file_browser.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

namespace {

// A folder with:
//   Music/  covers/  .hidden/
//   b-song.WAV  a-song.mp3  notes.txt  .secret.wav
class FileBrowserTest : public ::testing::Test {
  protected:
    fs::path root;

    void SetUp() override {
        root = fs::temp_directory_path() / ("rsans_browser_" + std::to_string(::getpid()));
        fs::remove_all(root);
        fs::create_directories(root / "Music" / "inner");
        fs::create_directories(root / "covers");
        fs::create_directories(root / ".hidden");
        for (const char* name : {"b-song.WAV", "a-song.mp3", "notes.txt", ".secret.wav", "Music/track.flac"}) {
            std::ofstream(root / name) << "x";
        }
    }

    void TearDown() override { fs::remove_all(root); }

    static std::vector<std::string> names(const FileBrowser& browser) {
        std::vector<std::string> result;
        for (const FileBrowser::Entry& entry : browser.entries()) {
            result.push_back(entry.name);
        }
        return result;
    }

    const std::vector<std::string> audio = {".wav", ".mp3", ".flac"};
};

} // namespace

TEST_F(FileBrowserTest, ListsParentThenFoldersThenMatchingFilesSorted) {
    const FileBrowser browser(audio, false, root.string());

    EXPECT_EQ(browser.directory(), root);
    EXPECT_EQ(names(browser), (std::vector<std::string>{"..", "covers", "Music", "a-song.mp3", "b-song.WAV"}));
    EXPECT_TRUE(browser.entries()[0].isParent);
    EXPECT_TRUE(browser.entries()[1].isDirectory);
    EXPECT_FALSE(browser.entries()[3].isDirectory);
    EXPECT_TRUE(browser.error().empty());
}

TEST_F(FileBrowserTest, ShowAllFilesListsOtherTypes) {
    FileBrowser browser(audio, false, root.string());

    browser.setShowAllFiles(true);

    EXPECT_EQ(names(browser).back(), "notes.txt");
}

TEST_F(FileBrowserTest, NoExtensionsListsEveryFile) {
    const FileBrowser browser({}, false, root.string());

    EXPECT_FALSE(browser.hasExtensionFilter());
    EXPECT_EQ(names(browser).size(), 6u);  // .., 2 folders, 3 files
}

TEST_F(FileBrowserTest, QueryFiltersIgnoringCaseAndDotShowsHidden) {
    FileBrowser browser(audio, false, root.string());

    browser.setQuery("SONG");
    EXPECT_EQ(names(browser), (std::vector<std::string>{"..", "a-song.mp3", "b-song.WAV"}));

    browser.setQuery(".s");
    EXPECT_EQ(names(browser), (std::vector<std::string>{"..", ".secret.wav"}));
}

TEST_F(FileBrowserTest, ActivateOpensFoldersGoesUpAndChoosesFiles) {
    FileBrowser browser(audio, false, root.string());
    browser.setQuery("mus");

    // Into Music (the search is cleared on the way)
    EXPECT_FALSE(browser.activate(1).has_value());
    EXPECT_EQ(browser.directory(), root / "Music");
    EXPECT_EQ(browser.query(), "");
    EXPECT_EQ(names(browser), (std::vector<std::string>{"..", "inner", "track.flac"}));

    // Choose the file
    EXPECT_EQ(browser.activate(2), (root / "Music" / "track.flac").string());

    // Back up
    EXPECT_FALSE(browser.activate(0).has_value());
    EXPECT_EQ(browser.directory(), root);

    EXPECT_FALSE(browser.activate(99).has_value());
}

TEST_F(FileBrowserTest, StartsInFolderOfCurrentPath) {
    EXPECT_EQ(FileBrowser(audio, false, (root / "Music" / "track.flac").string()).directory(), root / "Music");
    EXPECT_EQ(FileBrowser(audio, false, (root / "Music" / "gone.wav").string()).directory(), root / "Music");
    EXPECT_EQ(FileBrowser(audio, false, "/nonexistent/x/y.wav").directory(), fs::current_path());
    EXPECT_EQ(FileBrowser(audio, false, "").directory(), fs::current_path());
}

TEST_F(FileBrowserTest, SubmitQueryChoosesFirstMatchWhenOpening) {
    FileBrowser browser(audio, false, root.string());
    browser.setQuery("b-");

    EXPECT_EQ(browser.submitQuery(), (root / "b-song.WAV").string());
}

TEST_F(FileBrowserTest, SubmitQueryFollowsTypedPaths) {
    FileBrowser browser(audio, false, root.string());

    // A folder path opens it
    browser.setQuery((root / "Music").string() + "/");
    EXPECT_FALSE(browser.submitQuery().has_value());
    EXPECT_EQ(browser.directory(), root / "Music");

    // A relative file path chooses it
    browser.setQuery("../a-song.mp3");
    EXPECT_EQ(browser.submitQuery(), (root / "a-song.mp3").string());

    // A missing file does nothing
    browser.setQuery("/nonexistent/song.wav");
    EXPECT_FALSE(browser.submitQuery().has_value());
}

TEST_F(FileBrowserTest, SaveModeUsesQueryAsFileName) {
    // Given: the current output is root/old.mp4, which doesn't exist yet
    FileBrowser browser({".mp4"}, true, (root / "old.mp4").string());
    EXPECT_EQ(browser.query(), "old.mp4");
    EXPECT_EQ(names(browser).size(), 3u);  // The name doesn't filter: .., covers, Music

    // A folder name opens the folder and keeps nothing typed
    browser.setQuery("Music");
    EXPECT_FALSE(browser.submitQuery().has_value());
    EXPECT_EQ(browser.directory(), root / "Music");
    EXPECT_EQ(browser.query(), "");

    // Entering a folder from the list keeps the name being typed
    browser.setQuery("clip.mp4");
    browser.activate(1);  // inner
    EXPECT_EQ(browser.query(), "clip.mp4");

    // A new file name is chosen in the current folder
    EXPECT_EQ(browser.submitQuery(), (root / "Music" / "inner" / "clip.mp4").string());

    browser.setQuery("");
    EXPECT_FALSE(browser.submitQuery().has_value());
}

TEST_F(FileBrowserTest, UnreadableFolderReportsError) {
    FileBrowser browser(audio, false, root.string());

    browser.enterDirectory(root / "missing");

    EXPECT_FALSE(browser.error().empty());
    EXPECT_EQ(names(browser), std::vector<std::string>{".."});
    browser.goUp();
    EXPECT_EQ(browser.directory(), root);
}

TEST(ExpandHomeTest, ExpandHomeReplacesLeadingTilde) {
    const char* home = std::getenv("HOME");
    if (home == nullptr) {
        GTEST_SKIP() << "HOME not set";
    }
    EXPECT_EQ(expandHome("~/song.wav"), std::string(home) + "/song.wav");
    EXPECT_EQ(expandHome("~"), home);
    EXPECT_EQ(expandHome("a/~/b"), "a/~/b");
    EXPECT_EQ(expandHome("~user/x"), "~user/x");
}
