#include <rsans_args.h>
#include <rsans_ass.h>
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
            const std::string out  = tokenizeAudio(json);
            writeFile(args.analyze.output, out);
            break;
        }
        case CommandType::Rhyme: {
            const std::string json = readFile(args.rhyme.input);
            const std::string out  = detectRhymes(json);
            writeFile(args.rhyme.output, out);
            break;
        }
        case CommandType::Export: {
            const std::string json = readFile(args.exportOpt.input);
            const ProjectData data(json);
            const int code = exportVideo(data, args.exportOpt.output, args.ffmpegPath);
            return code;
        }
        case CommandType::Full: {
            std::string json = readFile(args.full.input);
            const ProjectData data(json);
            json = tokenizeAudio(json);
            json = detectRhymes(json);
            const int code = exportVideo(data, args.full.output, args.ffmpegPath);
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
