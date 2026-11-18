#include <rsans_export.h>

#include <rsans_ass.h>
#include <rsans_data.h>
#include <rsans_io.h>
#include <rsans_process.h>
#include <rsans_task.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <sstream>
#include <string>
#include <stdexcept>
#include <system_error>
#include <vector>

#include <cerrno>
#include <cstring>
#include <stdlib.h>  // mkdtemp

namespace  {

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

// Lines ffmpeg writes for -progress are "key=value" with no spaces, unlike
// its log lines
bool isProgressLine(const std::string& line) {
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos || equals == 0) {
        return false;
    }
    if (line.find(' ') != std::string::npos) {
        return false;
    }
    return std::all_of(line.begin(), line.begin() + static_cast<std::ptrdiff_t>(equals),
                       [](unsigned char c) { return std::islower(c) || std::isdigit(c) || c == '_'; });
}

// Turns ffmpeg's -progress lines into a fraction of the video's length
class FfmpegProgress {
  private:
    const ProgressHandler& d_onProgress;
    double d_durationUs;
    double d_lastFraction = -1.0;

    void report(double fraction) {
        fraction = std::clamp(fraction, 0.0, 1.0);
        if (fraction > d_lastFraction && d_onProgress) {
            d_lastFraction = fraction;
            d_onProgress(fraction);
        }
    }

  public:
    FfmpegProgress(const ProgressHandler& onProgress, double durationSeconds)
    : d_onProgress(onProgress)
    , d_durationUs(durationSeconds * 1'000'000.0) {}

    void handle(const std::string& line) {
        const std::size_t equals = line.find('=');
        const std::string key = line.substr(0, equals);
        const std::string value = line.substr(equals + 1);

        if (key == "progress" && value == "end") {
            report(1.0);
        } else if (key == "out_time_us" && d_durationUs > 0.0) {
            char* end = nullptr;
            const long long timeUs = std::strtoll(value.c_str(), &end, 10);
            if (end != value.c_str()) {  // Skips "N/A"
                report(static_cast<double>(timeUs) / d_durationUs);
            }
        }
    }
};

int executeFfmpegBuild(const ProjectData& data,
                       const std::string& outputVideoPath,
                       const std::string& ffmpegPath,
                       const std::string& assPath,
                       const std::string& fontsDir,
                       const FfmpegOutputHandler& onOutput,
                       const ProgressHandler& onProgress,
                       const CancelFlag* cancel) {
    std::ostringstream colorSource;
    colorSource << "color=c=" << escapeFilterValue(data.video.background)
                << ":s=" << data.video.width << "x" << data.video.height
                << ":d=" << data.audio.length;

    std::ostringstream filterGraph;
    filterGraph << "movie=" << escapeFilterValue(data.header.media) << ","
                << "scale=" << data.video.width << ":-1"
                << "[cover];[0:v][cover]"
                << "overlay=0:0,"
                << "subtitles=" << escapeFilterValue(assPath)
                << ":fontsdir=" << escapeFilterValue(fontsDir);

    // Arguments go straight to ffmpeg without a shell, so they need no quoting.
    // -nostdin: don't read keypresses meant for the caller (e.g. a TUI)
    // -nostats -progress pipe:1: replace the '\r' status line with
    //   machine-readable key=value progress lines on stdout
    const std::vector<std::string> args = {
        ffmpegPath, "-nostdin", "-nostats", "-progress", "pipe:1", "-y",
        "-f", "lavfi", "-i", colorSource.str(),
        "-i", data.audio.path,
        "-vf", filterGraph.str(),
        "-c:v", "libx264", "-c:a", "aac", "-shortest",
        outputVideoPath
    };

    constexpr std::size_t maxRecentLines = 10;
    std::deque<std::string> recentOutput;
    FfmpegProgress progress(onProgress, data.audio.length);

    const int result = runProcess(args, [&](const std::string& line) {
        if (isProgressLine(line)) {
            progress.handle(line);
            return;
        }
        if (onOutput) {
            onOutput(line);
        }
        recentOutput.push_back(line);
        if (recentOutput.size() > maxRecentLines) {
            recentOutput.pop_front();
        }
    }, cancel);

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

StagedFonts::StagedFonts(const std::vector<std::string>& fontPaths) {
    std::string pattern = (std::filesystem::temp_directory_path() / "rsans-fonts-XXXXXX").string();
    if (::mkdtemp(pattern.data()) == nullptr) {
        throw std::runtime_error("Can't make a temporary folder for the fonts: " +
                                 std::string(std::strerror(errno)));
    }
    d_dir = pattern;

    std::vector<std::filesystem::path> copied;  // Originals, to copy each once
    try {
        for (const std::string& path : fontPaths) {
            std::error_code ec;
            const std::filesystem::path original = std::filesystem::weakly_canonical(path, ec);
            const auto seen = std::find(copied.begin(), copied.end(), original);
            if (seen != copied.end()) {
                d_files.push_back(d_files[static_cast<std::size_t>(seen - copied.begin())]);
                continue;
            }
            const std::filesystem::path copy = std::filesystem::path(d_dir) /
                (std::to_string(copied.size()) + "-" + original.filename().string());
            std::filesystem::copy_file(path, copy, ec);
            if (ec) {
                throw std::runtime_error("Can't copy the font " + path + " for the export: " + ec.message());
            }
            copied.push_back(original);
            d_files.push_back(copy.string());
        }
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove_all(d_dir, ec);
        throw;
    }
}

StagedFonts::~StagedFonts() {
    std::error_code ec;
    std::filesystem::remove_all(d_dir, ec);
}

std::string subtitlePathFor(const std::string& outputVideoPath) {
    std::filesystem::path assPath(outputVideoPath);
    if (assPath.extension() == ".ass") {
        throw std::invalid_argument(
            "Output video path must not end in .ass: " + outputVideoPath);
    }
    return assPath.replace_extension(".ass").string();
}

int exportVideo(const ProjectData& data,
                const std::string& outputVideoPath,
                const std::string& ffmpegPath,
                const FfmpegOutputHandler& onOutput,
                const ProgressHandler& onProgress,
                const CancelFlag* cancel) {
    throwIfCancelled(cancel, "Export cancelled");

    const Ass assSubtitles(data);
    const std::string assPath = subtitlePathFor(outputVideoPath);

    writeFile(assPath, assSubtitles.text());

    try {
        // Copied now, for this render only, and deleted when it ends
        const StagedFonts fonts({data.header.fontPath, data.layout.fontPath});
        return executeFfmpegBuild(data, outputVideoPath, ffmpegPath, assPath, fonts.dir(),
                                  onOutput, onProgress, cancel);
    } catch (const CancelledError&) {
        // A cancelled export leaves no half-written video or its subtitles behind
        std::error_code ec;
        std::filesystem::remove(outputVideoPath, ec);
        std::filesystem::remove(assPath, ec);
        throw CancelledError("Export cancelled");
    }
}
