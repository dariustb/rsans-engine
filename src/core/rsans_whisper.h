#ifndef INCLUDED_RSANS_WHISPER
#define INCLUDED_RSANS_WHISPER

#include <rsans_data.h>

#include <string>
#include <vector>

ProjectData tokenizeAudio(const ProjectData& project);

// initialPrompt is forwarded to Whisper as initial_prompt to guide transcription.
// Pass the lyrics text here when a lyrics file is available; leave empty for
// free transcription (the default / fallback behaviour).
std::vector<Token> extractTokensFromAudio(
    const std::string& audioPath,
    const std::string& modelPath,
    const std::string& initialPrompt = ""
);

void mergeContractions(std::vector<Token>& tokens);

#endif
