#ifndef INCLUDED_RSANS_EXPORT
#define INCLUDED_RSANS_EXPORT

#include <functional>
#include <string>
#include <vector>
#include <rsans_data.h>
#include <rsans_task.h>

// Receives each line of ffmpeg's output (stdout and stderr combined).
using FfmpegOutputHandler = std::function<void(const std::string& line)>;

// Returns where the subtitle file for an output video is written: the same
// path with an .ass extension (e.g. "songs/out.mp4" -> "songs/out.ass").
// Throws std::invalid_argument if the output path itself ends in .ass.
std::string subtitlePathFor(const std::string& outputVideoPath);

// A temporary folder holding copies of some font files, for one export.
// ffmpeg's subtitles filter takes a single fonts folder, and libass loads only
// the files directly in it (not in subfolders), so fonts kept in different
// folders can't both be found from any one of them. Instead the fonts the
// video uses are copied into a fresh folder of their own. Each file is copied once, with a
// numbered prefix so two fonts with the same file name don't collide (libass
// finds fonts by the names inside them, not their file names). The folder and
// copies are deleted when this is destroyed.
// Throws std::runtime_error if the folder can't be made or a font copied.
class StagedFonts {
  private:
    std::string d_dir;
    std::vector<std::string> d_files;

  public:
    explicit StagedFonts(const std::vector<std::string>& fontPaths);
    ~StagedFonts();
    StagedFonts(const StagedFonts&) = delete;
    StagedFonts& operator=(const StagedFonts&) = delete;
    StagedFonts(StagedFonts&&) = delete;
    StagedFonts& operator=(StagedFonts&&) = delete;

    const std::string& dir() const { return d_dir; }
    // The copies, in the order given (a repeated path isn't copied again)
    const std::vector<std::string>& files() const { return d_files; }
};

// Renders the video. The subtitle file is written next to the output video
// (see subtitlePathFor) and kept afterwards. The two fonts are copied to a
// temporary folder for the render only (see StagedFonts).
// - onOutput: ffmpeg's log lines, captured rather than written to the terminal
// - onProgress: fraction of the video rendered so far, from 0.0 to 1.0
// - cancel: if requested, ffmpeg is stopped, the partial video and subtitle
//   file are deleted, and CancelledError is thrown
// Throws std::runtime_error if ffmpeg cannot be started or fails.
int exportVideo(const ProjectData& data,
                const std::string& outputVideoPath,
                const std::string& ffmpegPath,
                const FfmpegOutputHandler& onOutput = {},
                const ProgressHandler& onProgress = {},
                const CancelFlag* cancel = nullptr);

#endif
