#ifndef INCLUDED_RSANS_WHISPER
#define INCLUDED_RSANS_WHISPER

#include <rsans_data.h>

#include <string>
#include <vector>

ProjectData tokenizeAudio(const ProjectData& project);

std::vector<Token> extractTokensFromAudio(
    const std::string& audioPath,
    const std::string& modelPath
);

void mergeContractions(std::vector<Token>& tokens);

#endif
