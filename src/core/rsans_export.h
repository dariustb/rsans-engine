#ifndef INCLUDED_RSANS_EXPORT
#define INCLUDED_RSANS_EXPORT

#include <functional>
#include <string>
#include <rsans_data.h>

// Receives each line of ffmpeg's output (stdout and stderr combined).
using FfmpegOutputHandler = std::function<void(const std::string& line)>;

// Renders the video. ffmpeg's output is captured rather than written to the
// terminal, and passed line by line to onOutput if one is given.
// Throws std::runtime_error if ffmpeg cannot be started or fails.
int exportVideo(const ProjectData& data,
                const std::string& outputVideoPath,
                const std::string& ffmpegPath,
                const FfmpegOutputHandler& onOutput = {});

#endif
