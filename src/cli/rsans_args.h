#ifndef INCLUDED_RSANS_ARGS
#define INCLUDED_RSANS_ARGS

#include <string>

enum class CommandType {
    None,
    Analyze,
    Rhyme,
    Ass,
    Export,
    Full
};

struct CliOptions {
    CommandType command = CommandType::None;
    std::string ffmpegPath = "ffmpeg";

    struct AnalyzeOpts { std::string input; std::string output; } analyze;
    struct RhymeOpts   { std::string input; std::string output; } rhyme;
    struct AssOpts     { std::string input; std::string output; } ass;
    struct ExportOpts  { std::string input; std::string output; } exportOpt;
    struct FullOpts    { std::string input; std::string output; } full;
};

CliOptions parseArgs(int argc, char** argv);

#endif
