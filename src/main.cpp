#include <rsans_args.h>
#include <rsans_ass.h>
#include <rsans_data.h>
#include <rsans_export.h>
#include <rsans_io.h>
#include <rsans_rhyme.h>
#include <rsans_whisper.h>

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        const Args args = parseArgs(argc, argv);

        switch (args.command) {
        case CommandType::Analyze: {
            const std::string json = readFile(args.analyze.input);
            ProjectData data(json);
            tokenizeAudio(data);
            writeFile(args.analyze.output, data.toJson());
            break;
        }
        case CommandType::Rhyme: {
            const std::string json = readFile(args.rhyme.input);
            ProjectData data(json);
            if (!data.tokens.empty()) {
                detectRhymes(data);
                writeFile(args.rhyme.output, data.toJson());
            }
            break;
        }
        case CommandType::Export: {
            const std::string json = readFile(args.exportOpt.input);
            const ProjectData data(json);
            return exportVideo(data, args.exportOpt.output, args.ffmpegPath);
        }
        case CommandType::Full: {
            const std::string json = readFile(args.full.input);
            ProjectData data(json);
            tokenizeAudio(data);
            detectRhymes(data);
            return exportVideo(data, args.full.output, args.ffmpegPath);
        }
        default:
            std::cerr << "No valid command selected" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
