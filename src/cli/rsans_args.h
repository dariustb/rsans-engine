#ifndef INCLUDED_RSANS_ARGS
#define INCLUDED_RSANS_ARGS

#include <string>

enum class CommandType {
    None,
    Analyze,
    Rhyme,
    Export,
    Full,
    Tui,   // No subcommand, run from a terminal: open the interactive UI
    Help   // No subcommand, not run from a terminal: print helpText
};

struct Args {
    CommandType command = CommandType::None;
    std::string ffmpegPath = "ffmpeg";
    bool ffmpegGiven = false;  // --ffmpeg was passed (rather than the default)

    struct AnalyzeOpts { std::string input; std::string output; } analyze;
    struct RhymeOpts   { std::string input; std::string output; } rhyme;
    struct ExportOpts  { std::string input; std::string output; } exportOpt;
    struct FullOpts    { std::string input; std::string output; } full;

    std::string helpText;  // Set when command is Help
};

// Parses the command line. With no subcommand, the result is Tui when stdin
// and stdout are both a terminal, and Help otherwise (e.g. in a script or pipe).
Args parseArgs(int argc, char** argv);

// As above, with whether this is an interactive terminal given explicitly
Args parseArgs(int argc, char** argv, bool interactiveTerminal);

#endif
