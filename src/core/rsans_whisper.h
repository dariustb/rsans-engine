#ifndef INCLUDED_RSANS_WHISPER
#define INCLUDED_RSANS_WHISPER

#include <rsans_data.h>
#include <rsans_task.h>

#include <functional>
#include <string>
#include <vector>

enum class WhisperLogLevel { Debug, Info, Warn, Error };

// Receives each line whisper.cpp and ggml log while a transcription runs
using WhisperLogHandler = std::function<void(WhisperLogLevel level, const std::string& line)>;

// Transcribes the project's audio and returns a copy with its tokens replaced.
// - ffmpegPath: used to convert audio Whisper can't read directly (anything
//   but a 16 kHz mono/stereo WAV, e.g. MP3, FLAC, 44.1 kHz WAV) into a
//   temporary 16 kHz mono WAV, deleted afterwards. The project keeps pointing
//   at the original file, which export uses. If the project has no valid
//   audio.length, it is filled in from the converted audio.
// - onLog: whisper.cpp's log lines, captured rather than written to the
//   terminal. Whisper's logging is global to the process, so only one
//   transcription should run at a time.
// - onProgress: fraction transcribed so far, from 0.0 to 1.0
// - cancel: if requested, transcription stops and CancelledError is thrown.
//   Loading the model cannot be interrupted; cancellation takes effect after.
ProjectData tokenizeAudio(const ProjectData& project,
                          const std::string& ffmpegPath,
                          const WhisperLogHandler& onLog = {},
                          const ProgressHandler& onProgress = {},
                          const CancelFlag* cancel = nullptr);

// True if the file is a WAV that Whisper can read as-is: 16 kHz, mono or stereo
bool isWhisperReadyAudio(const std::string& audioPath);

// Converts any audio ffmpeg can read into a 16 kHz mono 16-bit WAV at
// outputPath, overwriting it. Throws std::runtime_error (with ffmpeg's error
// output) if conversion fails, or CancelledError if cancelled.
void convertAudioForWhisper(const std::string& inputPath,
                            const std::string& outputPath,
                            const std::string& ffmpegPath,
                            const CancelFlag* cancel = nullptr);

// Transcribes the audio and returns its words with timings. The audio must be
// a mono or stereo WAV file at 16 kHz. Throws std::runtime_error if the audio
// or model cannot be loaded, transcription fails, or no words are detected.
std::vector<Token> extractTokensFromAudio(
    const std::string& audioPath,
    const std::string& modelPath,
    const WhisperLogHandler& onLog = {},
    const ProgressHandler& onProgress = {},
    const CancelFlag* cancel = nullptr
);

void mergeContractions(std::vector<Token>& tokens);

#endif
