#ifndef INCLUDED_RSANS_WHISPER
#define INCLUDED_RSANS_WHISPER

#include <rsans_data.h>

#include <string>
#include <vector>

ProjectData tokenizeAudio(const ProjectData& project);

// Transcribes the audio and returns its words with timings. The audio must be
// a mono or stereo WAV file at 16 kHz. Throws std::runtime_error if the audio
// or model cannot be loaded, transcription fails, or no words are detected.
std::vector<Token> extractTokensFromAudio(
    const std::string& audioPath,
    const std::string& modelPath
);

void mergeContractions(std::vector<Token>& tokens);

#endif
