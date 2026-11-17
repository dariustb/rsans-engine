#ifndef INCLUDED_RSANS_EXPORT
#define INCLUDED_RSANS_EXPORT

#include <functional>
#include <string>
#include <rsans_data.h>

// Receives each line of ffmpeg's output (stdout and stderr combined).
using FfmpegOutputHandler = std::function<void(const std::string& line)>;

// Returns where the subtitle file for an output video is written: the same
// path with an .ass extension (e.g. "songs/out.mp4" -> "songs/out.ass").
// Throws std::invalid_argument if the output path itself ends in .ass.
std::string subtitlePathFor(const std::string& outputVideoPath);

// Renders the video. The subtitle file is written next to the output video
// (see subtitlePathFor) and kept afterwards. ffmpeg's output is captured
// rather than written to the terminal, and passed line by line to onOutput if
// one is given. Throws std::runtime_error if ffmpeg cannot be started or fails.
int exportVideo(const ProjectData& data,
                const std::string& outputVideoPath,
                const std::string& ffmpegPath,
                const FfmpegOutputHandler& onOutput = {});

#endif
