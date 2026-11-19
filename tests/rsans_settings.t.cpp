#include <rsans_settings.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <unistd.h>

namespace fs = std::filesystem;

namespace {

class SettingsTest : public ::testing::Test {
  protected:
    fs::path root = fs::temp_directory_path() / ("rsans_settings_t_" + std::to_string(::getpid()));
    void SetUp() override { fs::remove_all(root); }
    void TearDown() override { fs::remove_all(root); }
};

// Sets one environment variable, restoring it afterwards
struct EnvGuard {
    const char* name;
    std::optional<std::string> old;
    EnvGuard(const char* n, const char* value) : name(n) {
        if (const char* v = std::getenv(n)) {
            old = v;
        }
        value != nullptr ? ::setenv(n, value, 1) : ::unsetenv(n);
    }
    ~EnvGuard() { old ? ::setenv(name, old->c_str(), 1) : ::unsetenv(name); }
    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;
    EnvGuard(EnvGuard&&) = delete;
    EnvGuard& operator=(EnvGuard&&) = delete;
};

} // namespace

TEST_F(SettingsTest, SaveCreatesFolderAndLoadReadsBack) {
    const fs::path file = root / "a" / "b" / "settings.json";

    ASSERT_EQ(saveSettings(file.string(), {"/opt/ffmpeg", "~/whisper.cpp", "/opt/cmudict.dict"}), "");
    const AppSettings loaded = loadSettings(file.string());

    EXPECT_EQ(loaded.ffmpegPath, "/opt/ffmpeg");
    EXPECT_EQ(loaded.whisperDir, "~/whisper.cpp");
    EXPECT_EQ(loaded.cmudictPath, "/opt/cmudict.dict");
    EXPECT_FALSE(fs::exists(file.string() + ".tmp"));
}

TEST_F(SettingsTest, EmptyValuesAreLeftOut) {
    const fs::path file = root / "settings.json";
    ASSERT_EQ(saveSettings(file.string(), {"ffmpeg", ""}), "");

    std::ifstream in(file);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(text.find("whisperDir"), std::string::npos) << text;
    EXPECT_EQ(text.find("cmudictPath"), std::string::npos) << text;
    EXPECT_EQ(loadSettings(file.string()).whisperDir, "");
}

TEST_F(SettingsTest, MissingOrDamagedFileGivesEmptySettings) {
    EXPECT_EQ(loadSettings((root / "none.json").string()).ffmpegPath, "");

    fs::create_directories(root);
    std::ofstream(root / "bad.json") << "{ not json";
    EXPECT_EQ(loadSettings((root / "bad.json").string()).ffmpegPath, "");

    std::ofstream(root / "wrong.json") << R"({"ffmpegPath": 5, "whisperDir": "/w"})";
    const AppSettings wrong = loadSettings((root / "wrong.json").string());
    EXPECT_EQ(wrong.ffmpegPath, "");
    EXPECT_EQ(wrong.whisperDir, "/w");
}

TEST_F(SettingsTest, SaveReportsUnwritablePlace) {
    fs::create_directories(root);
    std::ofstream(root / "file") << "x";  // A file where a folder is needed

    EXPECT_NE(saveSettings((root / "file" / "settings.json").string(), {"ffmpeg", ""}), "");
}

TEST(SettingsPathTest, DefaultSettingsPathFollowsXdgThenHome) {
    {
        EnvGuard xdg("XDG_CONFIG_HOME", "/xdg");
        EXPECT_EQ(defaultSettingsPath(), "/xdg/rsans/settings.json");
    }
    EnvGuard noXdg("XDG_CONFIG_HOME", nullptr);
    EnvGuard home("HOME", "/home/someone");
    EXPECT_EQ(defaultSettingsPath(), "/home/someone/.config/rsans/settings.json");
    EnvGuard noHome("HOME", nullptr);
    EXPECT_EQ(defaultSettingsPath(), "");
}
