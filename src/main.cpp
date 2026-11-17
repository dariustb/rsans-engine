#include <rsans_args.h>
#include <rsans_ass.h>
#include <rsans_data.h>
#include <rsans_export.h>
#include <rsans_io.h>
#include <rsans_rhyme.h>
#include <rsans_whisper.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Throws with every problem listed if a step's checks found any
void requireReady(const std::vector<ValidationError>& errors, const std::string& step) {
    if (errors.empty()) {
        return;
    }
    std::string message = "Project is not ready to " + step + ":";
    for (const ValidationError& error : errors) {
        message += "\n  " + error.field + ": " + error.message;
    }
    throw std::runtime_error(message);
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Args args = parseArgs(argc, argv);

        switch (args.command) {
        case CommandType::Analyze: {
            const std::string json = readFile(args.analyze.input);
            const ProjectData data(json);
            requireReady(data.validateForAnalyze(), "analyze");
            const ProjectData tokenizedData = tokenizeAudio(data);
            writeFile(args.analyze.output, tokenizedData.toJson());
            break;
        }
        case CommandType::Rhyme: {
            const std::string json = readFile(args.rhyme.input);
            const ProjectData data(json);
            requireReady(data.validateForRhyme(), "detect rhymes");
            const ProjectData rhymedData = detectRhymes(data);
            writeFile(args.rhyme.output, rhymedData.toJson());
            break;
        }
        case CommandType::Export: {
            const std::string json = readFile(args.exportOpt.input);
            const ProjectData data(json);
            requireReady(data.validateForExport(), "export");
            const int code = exportVideo(data, args.exportOpt.output, args.ffmpegPath);
            return code;
        }
        case CommandType::Full: {
            const std::string json = readFile(args.full.input);
            const ProjectData data(json);
            requireReady(data.validateForFull(), "run");
            const ProjectData tokenizedData = tokenizeAudio(data);
            const ProjectData rhymedData = detectRhymes(tokenizedData);
            const int code = exportVideo(rhymedData, args.full.output, args.ffmpegPath);
            return code;
        }
        default:
            std::cerr << "No command selected" << std::endl;
            return 1;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
