#ifndef INCLUDED_RSANS_EXPORT
#define INCLUDED_RSANS_EXPORT

#include <string>

int exportVideo(const std::string& projectJson,
                const std::string& outputVideoPath,
                const std::string& ffmpegPath);

#endif
