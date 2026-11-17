#include <rsans_export.h>

#include <rsans_ass.h>
#include <rsans_data.h>
#include <rsans_io.h>

#include <array>
#include <algorithm>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <sstream>
#include <string>
#include <stdexcept>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h>
#endif

namespace  {

bool isImageFile(const std::string& path)
{
    std::filesystem::path p(path);
    if (!p.has_extension())
        return false;

    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                    [](unsigned char c){ return std::tolower(c); });

    static constexpr std::array<const char*, 7> imageExts = {
        ".png", ".jpg", ".jpeg", ".bmp", ".webp", ".tif", ".tiff"
    };

    return std::any_of(imageExts.begin(), imageExts.end(),
                        [&](const char* e){ return ext == e; });
}

// Quotes one argument so the shell passes it through unchanged, whatever
// spaces or special characters it contains.
std::string shellQuote(const std::string& arg) {
#ifdef _WIN32
    // cmd.exe: wrap in double quotes (Windows paths cannot contain '"')
    return "\"" + arg + "\"";
#else
    // POSIX sh: wrap in single quotes; an embedded ' becomes '\''
    std::string quoted = "'";
    for (const char c : arg) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted += c;
        }
    }
    quoted += "'";
    return quoted;
#endif
}

// Escapes a value placed inside an ffmpeg filtergraph (e.g. movie=<path>).
// ffmpeg parses these in two passes, each with its own special characters,
// so the value is escaped once per pass. See "Notes on filtergraph escaping"
// in the ffmpeg-filters documentation.
std::string escapeFilterValue(const std::string& value) {
    const auto escapeChars = [](const std::string& input, const std::string& special) {
        std::string escaped;
        for (const char c : input) {
            if (special.find(c) != std::string::npos) {
                escaped += '\\';
            }
            escaped += c;
        }
        return escaped;
    };

    const std::string optionLevel = escapeChars(value, "\\':");
    return escapeChars(optionLevel, "\\'[],;");
}

// Runs a shell command, capturing stdout and stderr instead of letting them
// reach the terminal. Each line is passed to onOutput; ffmpeg ends its progress
// updates with '\r', so that counts as a line break too. Returns the exit code
// and keeps the last few lines in recentOutput for error messages.
int runCapturingOutput(const std::string& command,
                       const FfmpegOutputHandler& onOutput,
                       std::deque<std::string>& recentOutput) {
    constexpr std::size_t maxRecentLines = 10;

    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    if (pipe == nullptr) {
        throw std::runtime_error("failed to start ffmpeg");
    }

    const auto emitLine = [&](std::string& line) {
        if (line.empty()) {
            return;
        }
        if (onOutput) {
            onOutput(line);
        }
        recentOutput.push_back(line);
        if (recentOutput.size() > maxRecentLines) {
            recentOutput.pop_front();
        }
        line.clear();
    };

    std::array<char, 4096> buffer{};
    std::string line;
    std::size_t bytesRead = 0;
    while ((bytesRead = std::fread(buffer.data(), 1, buffer.size(), pipe)) > 0) {
        for (std::size_t i = 0; i < bytesRead; ++i) {
            const char c = buffer[i];
            if (c == '\n' || c == '\r') {
                emitLine(line);
            } else {
                line += c;
            }
        }
    }
    emitLine(line);

    const int status = pclose(pipe);
#ifdef _WIN32
    return status;
#else
    if (status == -1) {
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

int executeFfmpegBuild(const ProjectData& data,
                       const std::string& outputVideoPath,
                       const std::string& ffmpegPath,
                       const std::string& assPath,
                       const FfmpegOutputHandler& onOutput) {
    // Build ffmpeg command
    const std::filesystem::path layoutFontDir = std::filesystem::path(data.layout.fontPath).parent_path();
    const std::filesystem::path headerFontDir = std::filesystem::path(data.header.fontPath).parent_path();
    // Use the common ancestor so fontconfig (used by ffmpeg's libass) can discover all fonts
    // TODO: this only works when the fonts share the same immediate parent folder
    const std::filesystem::path fontsDir = (layoutFontDir == headerFontDir)
        ? layoutFontDir
        : layoutFontDir.parent_path();

    std::ostringstream colorSource;
    colorSource << "color=c=" << escapeFilterValue(data.video.background)
                << ":s=" << data.video.width << "x" << data.video.height
                << ":d=" << data.audio.length;

    std::ostringstream filterGraph;
    filterGraph << "movie=" << escapeFilterValue(data.header.media) << ","
                << "scale=" << data.video.width << ":-1"
                << (isImageFile(data.header.media) ? "[cover];[0:v][cover]" : "[hdr];[0:v][hdr]")
                << "overlay=0:0,"
                << "subtitles=" << escapeFilterValue(assPath)
                << ":fontsdir=" << escapeFilterValue(fontsDir.string());

    // Every value that comes from the project or the caller is quoted, so paths
    // containing spaces or shell characters reach ffmpeg as single arguments
    std::ostringstream cmd;
    // -nostdin stops ffmpeg from reading keypresses meant for the caller (e.g. a TUI)
    cmd << shellQuote(ffmpegPath) << " -nostdin -y ";
    cmd << "-f lavfi -i " << shellQuote(colorSource.str()) << " ";
    cmd << "-i " << shellQuote(data.audio.path) << " ";
    cmd << "-vf " << shellQuote(filterGraph.str()) << " ";
    cmd << "-c:v libx264 -c:a aac -shortest ";
    cmd << shellQuote(outputVideoPath);

    // Execute ffmpeg
    std::deque<std::string> recentOutput;
    const int result = runCapturingOutput(cmd.str(), onOutput, recentOutput);

    if (result != 0) {
        std::string message = "ffmpeg command failed with exit code " +
                              std::to_string(result);
        if (!recentOutput.empty()) {
            message += "\nLast ffmpeg output:";
            for (const std::string& outputLine : recentOutput) {
                message += "\n  " + outputLine;
            }
        }
        throw std::runtime_error(message);
    }

    return 0;
}

}

int exportVideo(const ProjectData& data,
                const std::string& outputVideoPath,
                const std::string& ffmpegPath,
                const FfmpegOutputHandler& onOutput) {
    const Ass assSubtitles(data);
    const std::string assPath = "lyrics.ass";
    
    writeFile(assPath, assSubtitles.text());

    return executeFfmpegBuild(data, outputVideoPath, ffmpegPath, assPath, onOutput);
}
