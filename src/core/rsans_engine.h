#ifndef INCLUDED_RSANS_ENGINE
#define INCLUDED_RSANS_ENGINE

#include <string>

std::string tokenizeAudio(const std::string& projectJson);
std::string detectRhymes(const std::string& projectJson);
std::string generateAss(const std::string& projectJson);
int exportVideo(
    const std::string& projectJson,
    const std::string& outputVideoPath,
    const std::string& ffmpegPath
);

#endif