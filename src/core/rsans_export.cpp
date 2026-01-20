#include <rsans_export.h>

#include <rsans_ass.h>
#include <rsans_data.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

int exportVideo(const ProjectData& data,
                const std::string& outputVideoPath,
                const std::string& ffmpegPath) {
    // Generate ASS subtitles
    std::string assContent = generateAss(data);

    // Write to temporary .ass file
    std::string tempAssFile = "lyrics.ass";
    std::ofstream assFile(tempAssFile);
    if (!assFile.is_open()) {
        throw std::runtime_error("Failed to create temporary .ass file");
    }
    assFile << assContent;
    assFile.close();

    // Remove '#' from background color for ffmpeg
    std::string colorValue = data.video.background.substr(1);

    // Build ffmpeg command
    std::ostringstream cmd;
    cmd << ffmpegPath << " -y ";
    cmd << "-f lavfi -i color=c=" << data.video.background << ":s=" << data.video.width << "x"
        << data.video.height << ":d=" << data.audio.length << " ";
    cmd << "-i " << data.audio.path << " ";
    cmd << "-vf subtitles=" << tempAssFile << " ";
    cmd << "-c:v libx264 -c:a aac ";
    cmd << outputVideoPath;
    
    // Execute ffmpeg
    int result = std::system(cmd.str().c_str());
    std::cout << "~~~" << cmd.str() << std::endl;
    
    if (result != 0) {
        throw std::runtime_error("ffmpeg command failed with exit code " +
                                 std::to_string(result));
    }

    return 0;
}
