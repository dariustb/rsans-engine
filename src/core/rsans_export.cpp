#include <rsans_export.h>

#include <rsans_ass.h>
#include <rsans_data.h>
#include <rsans_io.h>

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace  {

int executeFfmpegBuild(const ProjectData& data,
                       const std::string& outputVideoPath,
                       const std::string& ffmpegPath,
                       const std::string& assPath) {
    // Build ffmpeg command
    std::filesystem::path fontPath(data.layout.fontPath);
    std::ostringstream cmd;
    cmd << ffmpegPath << " -y ";
    cmd << "-f lavfi -i color=c=" << data.video.background << ":s=" << data.video.width << "x"
        << data.video.height << ":d=" << data.audio.length << " ";
    cmd << "-i " << data.audio.path << " ";
    cmd << "-vf subtitles=" << assPath << ":fontsdir=" << fontPath.parent_path().string() << " ";
    cmd << "-c:v libx264 -c:a aac ";
    cmd << outputVideoPath;
    
    // Execute ffmpeg
    const int result = std::system(cmd.str().c_str());
    
    if (result != 0) {
        throw std::runtime_error("ffmpeg command failed with exit code " +
                                 std::to_string(result));
    }

    return 0;
}

}

int exportVideo(const ProjectData& data,
                const std::string& outputVideoPath,
                const std::string& ffmpegPath) {
    const Ass assSubtitles(data);
    const std::string assPath = "lyrics.ass";
    
    writeFile(assPath, assSubtitles.text());

    return executeFfmpegBuild(data, outputVideoPath, ffmpegPath, assPath);
}
