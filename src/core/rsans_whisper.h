#ifndef INCLUDED_RSANS_WHISPER
#define INCLUDED_RSANS_WHISPER

#include <rsans_data.h>

#include <functional>
#include <string>
#include <vector>

enum class WhisperLogLevel { Debug, Info, Warn, Error };

// Receives each line whisper.cpp and ggml log while a transcription runs
using WhisperLogHandler = std::function<void(WhisperLogLevel level, const std::string& line)>;

// Whisper's log output is captured rather than written to the terminal, and
// passed line by line to onLog if one is given. Whisper's logging is global to
// the process, so only one transcription should run at a time.
ProjectData tokenizeAudio(const ProjectData& project,
                          const WhisperLogHandler& onLog = {});

// Transcribes the audio and returns its words with timings. The audio must be
// a mono or stereo WAV file at 16 kHz. Throws std::runtime_error if the audio
// or model cannot be loaded, transcription fails, or no words are detected.
std::vector<Token> extractTokensFromAudio(
    const std::string& audioPath,
    const std::string& modelPath,
    const WhisperLogHandler& onLog = {}
);

void mergeContractions(std::vector<Token>& tokens);

#endif
