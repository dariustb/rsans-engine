#include <rsans_export.h>

#include <rsans_ass.h>

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

int exportVideo(const std::string& projectJson,
                const std::string& outputVideoPath,
                const std::string& ffmpegPath) {
    // Parse JSON to get video config and calculate duration
    json j = json::parse(projectJson);

    std::string audioPath = j["audioPath"].get<std::string>();
    int width = j["video"]["width"].get<int>();
    int height = j["video"]["height"].get<int>();
    std::string background = j["video"]["background"].get<std::string>();

    // Calculate duration from tokens
    int maxEndMs = 0;
    for (const auto& token : j["tokens"]) {
        int endMs = token["endMs"].get<int>();
        if (endMs > maxEndMs) {
            maxEndMs = endMs;
        }
    }

    // Add buffer and convert to seconds
    double durationSeconds = (maxEndMs + 1000) / 1000.0;

    // Generate ASS subtitles
    std::string assContent = generateAss(projectJson);

    // Write to temporary .ass file
    std::string tempAssFile = "lyrics.ass";
    std::ofstream assFile(tempAssFile);
    if (!assFile.is_open()) {
        throw std::runtime_error("Failed to create temporary .ass file");
    }
    assFile << assContent;
    assFile.close();

    // Remove '#' from background color for ffmpeg
    std::string colorValue = background.substr(1);

    // Build ffmpeg command
    std::ostringstream cmd;
    cmd << ffmpegPath << " -y ";
    cmd << "-f lavfi -i color=c=" << background << ":s=" << width << "x"
        << height << ":d=" << durationSeconds << " ";
    cmd << "-i " << audioPath << " ";
    cmd << "-vf subtitles=" << tempAssFile << " ";
    cmd << "-c:v libx264 -c:a aac ";
    cmd << outputVideoPath;

    std::cout << cmd.str() << std::endl;

    // Execute ffmpeg
    int result = std::system(cmd.str().c_str());

    if (result != 0) {
        throw std::runtime_error("ffmpeg command failed with exit code " +
                                 std::to_string(result));
    }

    return 0;
}
