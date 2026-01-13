#include <rsans_args.h>

#include <CLI/CLI.hpp>

#include <cstdlib>
#include <string>

namespace {
    const std::string APP_DESCRIPTION = "RSANS -- offline audio-aligned lyrics visualization engine";
}

Args parseArgs(int argc, char** argv) {
    Args opts;

    CLI::App app{APP_DESCRIPTION};

    app.add_option("--ffmpeg", opts.ffmpegPath, "Path to ffmpeg binary")
       ->capture_default_str();

    // analyze
    CLI::App* analyze = app.add_subcommand("analyze", "Transcribe audio and populate tokens");
    analyze->add_option("input", opts.analyze.input, "Input project JSON")->required();
    analyze->add_option("-o,--out", opts.analyze.output, "Output project JSON")->required();

    // rhyme
    CLI::App* rhyme = app.add_subcommand("rhyme", "Detect rhymes and tag tokens");
    rhyme->add_option("input", opts.rhyme.input, "Input project JSON")->required();
    rhyme->add_option("-o,--out", opts.rhyme.output, "Output project JSON")->required();

    // export
    CLI::App* exportCmd = app.add_subcommand("export", "Render final video");
    exportCmd->add_option("input", opts.exportOpt.input, "Input project JSON")->required();
    exportCmd->add_option("-o,--out", opts.exportOpt.output, "Output video file")->required();

    // full
    CLI::App* full = app.add_subcommand("full", "Analyze, detect rhymes, and export");
    full->add_option("input", opts.full.input, "Input project JSON")->required();
    full->add_option("-o,--out", opts.full.output, "Output video file")->required();

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e){
        std::exit(app.exit(e));
    }

    if (analyze->parsed()) {
        opts.command = CommandType::Analyze;
    } else if (rhyme->parsed()) {
        opts.command = CommandType::Rhyme;
    } else if (exportCmd->parsed()) {
        opts.command = CommandType::Export;
    } else if (full->parsed()) {
        opts.command = CommandType::Full;
    } else {
        throw std::runtime_error("No subcommand specified");
    }

    return opts;
}
