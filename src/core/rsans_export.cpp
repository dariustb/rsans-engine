#include <rsans_export.h>

#include <rsans_ass.h>
#include <rsans_data.h>
#include <rsans_io.h>

#include <array>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <stdexcept>

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
    cmd << "-vf " << "\"movie=" << data.header.media << ","
        << "scale=" << data.video.width << ":-1"
        << (isImageFile(data.header.media) ? "[cover];[0:v][cover]" : "[hdr];[0:v][hdr]") << "overlay=0:0,";
    cmd << "subtitles=" << assPath << ":fontsdir=" << fontPath.parent_path().string() << "\" ";
    cmd << "-c:v libx264 -c:a aac -shortest ";
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
