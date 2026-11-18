#include <rsans_args.h>

#include <CLI/CLI.hpp>

#include <cstdlib>
#include <string>

#include <unistd.h>

namespace {
    const std::string APP_DESCRIPTION =
        "RSANS Lyric Alignment & Visualization Engine\n\n"
        "Transcribe audio, detect rhyme schemes, and render synchronized\n"
        "lyrics videos with color-coded rhyme highlighting.";
}

Args parseArgs(int argc, char** argv) {
    const bool interactiveTerminal = isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
    return parseArgs(argc, argv, interactiveTerminal);
}

Args parseArgs(int argc, char** argv, bool interactiveTerminal) {
    Args opts;

    CLI::App app{APP_DESCRIPTION};

    CLI::Option* ffmpegOption = app.add_option("--ffmpeg", opts.ffmpegPath, "Path to ffmpeg binary")
       ->capture_default_str();

    // analyze
    CLI::App* analyze = app.add_subcommand("analyze", "Transcribe the audio into timed words, saved in the project");
    analyze->add_option("-i,--input", opts.analyze.input, "Project file to read (.rsans)")->required();
    analyze->add_option("-o,--out", opts.analyze.output, "Project file to write (.rsans); can be the input file")->required();

    // rhyme
    CLI::App* rhyme = app.add_subcommand("rhyme", "Find the rhyming words and give each rhyme a color");
    rhyme->add_option("-i,--input", opts.rhyme.input, "Project file to read (.rsans)")->required();
    rhyme->add_option("-o,--out", opts.rhyme.output, "Project file to write (.rsans); can be the input file")->required();

    // export
    CLI::App* exportCmd = app.add_subcommand("export", "Render the lyric video");
    exportCmd->add_option("-i,--input", opts.exportOpt.input, "Project file to read (.rsans)")->required();
    exportCmd->add_option("-o,--out", opts.exportOpt.output, "Video to write, e.g. song.mp4 (subtitles go beside it as .ass)")->required();

    // full
    CLI::App* full = app.add_subcommand("full", "Analyze, rhyme and export in one go; the project isn't saved");
    full->add_option("-i,--input", opts.full.input, "Project file to read (.rsans)")->required();
    full->add_option("-o,--out", opts.full.output, "Video to write, e.g. song.mp4 (subtitles go beside it as .ass)")->required();

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e){
        std::exit(app.exit(e));
    }
    opts.ffmpegGiven = ffmpegOption->count() > 0;

    if (analyze->parsed()) {
        opts.command = CommandType::Analyze;
    } else if (rhyme->parsed()) {
        opts.command = CommandType::Rhyme;
    } else if (exportCmd->parsed()) {
        opts.command = CommandType::Export;
    } else if (full->parsed()) {
        opts.command = CommandType::Full;
    } else if (interactiveTerminal) {
        opts.command = CommandType::Tui;
    } else {
        opts.command = CommandType::Help;
        opts.helpText = app.help();
    }

    return opts;
}
