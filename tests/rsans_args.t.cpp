#include <rsans_args.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

// Builds a mutable argv from strings, with "rsans" as the program name
class Argv {
  private:
    std::vector<std::string> d_args;
    std::vector<char*> d_pointers;

  public:
    explicit Argv(std::vector<std::string> args) : d_args(std::move(args)) {
        d_args.insert(d_args.begin(), "rsans");
        for (std::string& arg : d_args) {
            d_pointers.push_back(arg.data());
        }
        d_pointers.push_back(nullptr);
    }

    int argc() const { return static_cast<int>(d_args.size()); }
    char** argv() { return d_pointers.data(); }
};

} // namespace

TEST(ArgsTest, ParseArgsReturnsTuiGivenNoSubcommandInTerminal) {
    // Given
    Argv args({});

    // When
    const Args result = parseArgs(args.argc(), args.argv(), true);

    // Then
    EXPECT_EQ(result.command, CommandType::Tui);
}

TEST(ArgsTest, ParseArgsReturnsHelpGivenNoSubcommandOutsideTerminal) {
    // Given
    Argv args({});

    // When
    const Args result = parseArgs(args.argc(), args.argv(), false);

    // Then
    EXPECT_EQ(result.command, CommandType::Help);
    EXPECT_NE(result.helpText.find("analyze"), std::string::npos);
    EXPECT_NE(result.helpText.find("export"), std::string::npos);
}

TEST(ArgsTest, ParseArgsKeepsFfmpegPathGivenTuiLaunch) {
    // Given
    Argv args({"--ffmpeg", "/opt/ffmpeg/bin/ffmpeg"});

    // When
    const Args result = parseArgs(args.argc(), args.argv(), true);

    // Then
    EXPECT_EQ(result.command, CommandType::Tui);
    EXPECT_EQ(result.ffmpegPath, "/opt/ffmpeg/bin/ffmpeg");
}

TEST(ArgsTest, ParseArgsDefaultsFfmpegToPathLookupGivenNoFlag) {
    Argv args({});
    EXPECT_EQ(parseArgs(args.argc(), args.argv(), true).ffmpegPath, "ffmpeg");
}

TEST(ArgsTest, ParseArgsRunsSubcommandGivenSubcommandInTerminal) {
    // Given: a subcommand always wins over opening the TUI
    Argv args({"analyze", "-i", "in.rsans", "-o", "out.rsans"});

    // When
    const Args result = parseArgs(args.argc(), args.argv(), true);

    // Then
    EXPECT_EQ(result.command, CommandType::Analyze);
    EXPECT_EQ(result.analyze.input, "in.rsans");
    EXPECT_EQ(result.analyze.output, "out.rsans");
}

TEST(ArgsTest, ParseArgsRunsSubcommandGivenSubcommandOutsideTerminal) {
    // Given
    Argv args({"full", "-i", "in.rsans", "-o", "out.mp4"});

    // When
    const Args result = parseArgs(args.argc(), args.argv(), false);

    // Then
    EXPECT_EQ(result.command, CommandType::Full);
    EXPECT_EQ(result.full.input, "in.rsans");
    EXPECT_EQ(result.full.output, "out.mp4");
}

TEST(ArgsTest, ParseArgsRecordsWhetherFfmpegWasGiven) {
    Argv given({"--ffmpeg", "/opt/ffmpeg"});
    EXPECT_TRUE(parseArgs(given.argc(), given.argv(), true).ffmpegGiven);

    Argv none({});
    EXPECT_FALSE(parseArgs(none.argc(), none.argv(), true).ffmpegGiven);
}

TEST(ArgsTest, HelpTextDescribesStepsInPlainWords) {
    Argv args({});

    const std::string help = parseArgs(args.argc(), args.argv(), false).helpText;

    EXPECT_NE(help.find("Transcribe the audio into timed words"), std::string::npos) << help;
    EXPECT_EQ(help.find("JSON"), std::string::npos) << help;
    EXPECT_EQ(help.find("tokens"), std::string::npos) << help;
}
