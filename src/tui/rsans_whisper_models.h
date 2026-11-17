#ifndef INCLUDED_RSANS_WHISPER_MODELS
#define INCLUDED_RSANS_WHISPER_MODELS

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The Whisper models whisper.cpp can download, read from a whisper.cpp
// checkout rather than hard-coded, so the list stays current:
//   - names from models/download-ggml-model.sh (its `models="..."` list)
//   - sizes from the disk-size table in models/README.md
// Models are stored where that script puts them: <whisper.cpp>/models.
struct WhisperModel {
    std::string name;  // e.g. "base.en"
    std::string path;  // <whisper.cpp>/models/ggml-<name>.bin
    bool installed = false;
    std::optional<std::uint64_t> sizeBytes;  // Download size, if known
    bool sizeEstimated = false;              // Worked out from a related model
};

// The whisper.cpp checkout to use: $WHISPER_CPP_DIR if set, otherwise the one
// chosen when building (RSANS_WHISPER_CPP_DIR, by default the copy CMake
// downloaded to build whisper.cpp)
std::string defaultWhisperDir();

std::string whisperModelsDir(const std::string& whisperDir);
std::string whisperDownloadScript(const std::string& whisperDir);

// Every downloadable model, in the script's order. Empty if whisperDir isn't
// a whisper.cpp checkout (no download script).
std::vector<WhisperModel> listWhisperModels(const std::string& whisperDir);

// The models worth offering by default: one per size, English-only where
// there is one (RSANS transcribes English), the newest large models, and no
// quantized or tinydiarize variants. Installed models are always included.
// Returns indexes into `models`, in order.
std::vector<std::size_t> mainWhisperModels(const std::vector<WhisperModel>& models);

// The parts, exposed for testing
std::vector<std::string> parseModelNames(const std::string& script);
std::vector<std::pair<std::string, std::uint64_t>> parseModelSizes(const std::string& readme);

// "75 MiB", "1.5 GiB"
std::string formatSize(std::uint64_t bytes);

// "medium (1.5 GiB)", "large-v3-q5_0 (~1.1 GiB)" or "base.en ✓"
std::string modelLabel(const WhisperModel& model);

// Download progress (0-1) from a line of curl, wget or wget2 output, if the
// line is a progress update
std::optional<double> parseDownloadProgress(const std::string& line);

#endif
