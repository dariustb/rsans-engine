#include <rsans_whisper_models.h>

#include <rsans_file_browser.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifndef RSANS_WHISPER_CPP_DIR
#define RSANS_WHISPER_CPP_DIR ""
#endif

namespace fs = std::filesystem;

namespace {

std::string readText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

bool isInstalled(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec) && fs::file_size(path, ec) > 0 && !ec;
}

// Quantized models aren't all in the README's table, so their size is worked
// out from the full model's: about 37% for 5-bit and 53% for 8-bit, the
// ratios of the quantized models the table does list
std::optional<std::uint64_t> estimateSize(const std::string& name,
                                          const std::map<std::string, std::uint64_t>& sizes) {
    static const std::array<std::pair<const char*, double>, 3> ratios = {{
        {"-q5_0", 0.37}, {"-q5_1", 0.37}, {"-q8_0", 0.53},
    }};
    for (const auto& [suffix, ratio] : ratios) {
        const std::string tail(suffix);
        if (name.size() > tail.size() && name.compare(name.size() - tail.size(), tail.size(), tail) == 0) {
            const auto full = sizes.find(name.substr(0, name.size() - tail.size()));
            if (full != sizes.end()) {
                return static_cast<std::uint64_t>(static_cast<double>(full->second) * ratio);
            }
        }
    }
    return std::nullopt;
}

} // namespace

std::string defaultWhisperDir() {
    const char* fromEnvironment = std::getenv("WHISPER_CPP_DIR");
    if (fromEnvironment != nullptr && *fromEnvironment != '\0') {
        return expandHome(fromEnvironment);
    }
    return RSANS_WHISPER_CPP_DIR;
}

std::string whisperModelsDir(const std::string& whisperDir) {
    return (fs::path(expandHome(whisperDir)) / "models").string();
}

std::string whisperDownloadScript(const std::string& whisperDir) {
    return (fs::path(whisperModelsDir(whisperDir)) / "download-ggml-model.sh").string();
}

std::vector<std::string> parseModelNames(const std::string& script) {
    // models="tiny
    // tiny.en
    // ..."
    static const std::regex list(R"(^models="([^"]*)\")", std::regex::multiline);
    std::smatch match;
    std::vector<std::string> names;
    if (!std::regex_search(script, match, list)) {
        return names;
    }
    std::istringstream words(match[1].str());
    std::string name;
    while (words >> name) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::pair<std::string, std::uint64_t>> parseModelSizes(const std::string& readme) {
    // | base.en             | 142 MiB | `137c...` |
    static const std::regex row(R"(^\|\s*([A-Za-z0-9._-]+)\s*\|\s*([0-9.]+)\s*(MiB|GiB|MB|GB)\s*\|)");
    std::vector<std::pair<std::string, std::uint64_t>> sizes;
    std::istringstream lines(readme);
    std::string line;
    while (std::getline(lines, line)) {
        std::smatch match;
        if (std::regex_search(line, match, row)) {
            const double amount = std::stod(match[2].str());
            const std::string unit = match[3].str();
            const double scale = unit[0] == 'G' ? 1024.0 * 1024.0 * 1024.0 : 1024.0 * 1024.0;
            sizes.emplace_back(match[1].str(), static_cast<std::uint64_t>(amount * scale));
        }
    }
    return sizes;
}

std::vector<WhisperModel> listWhisperModels(const std::string& whisperDir) {
    std::vector<WhisperModel> models;
    if (whisperDir.empty()) {
        return models;
    }
    const std::string modelsDir = whisperModelsDir(whisperDir);
    const std::vector<std::string> names = parseModelNames(readText(whisperDownloadScript(whisperDir)));

    std::map<std::string, std::uint64_t> sizes;
    for (const auto& [name, bytes] : parseModelSizes(readText((fs::path(modelsDir) / "README.md").string()))) {
        sizes[name] = bytes;
    }

    for (const std::string& name : names) {
        WhisperModel model;
        model.name = name;
        model.path = (fs::path(modelsDir) / ("ggml-" + name + ".bin")).string();
        model.installed = isInstalled(model.path);
        const auto known = sizes.find(name);
        if (known != sizes.end()) {
            model.sizeBytes = known->second;
        } else {
            model.sizeBytes = estimateSize(name, sizes);
            model.sizeEstimated = model.sizeBytes.has_value();
        }
        models.push_back(std::move(model));
    }
    return models;
}

std::vector<std::size_t> mainWhisperModels(const std::vector<WhisperModel>& models) {
    const auto exists = [&](const std::string& name) {
        return std::any_of(models.begin(), models.end(), [&](const WhisperModel& m) { return m.name == name; });
    };
    // large-v<N><rest>: only the highest N for each rest ("", "-turbo")
    static const std::regex largeVersion(R"(^large-v(\d+)(.*)$)");
    const auto superseded = [&](const std::string& name) {
        std::smatch match;
        if (!std::regex_match(name, match, largeVersion)) {
            return false;
        }
        const int version = std::stoi(match[1].str());
        return std::any_of(models.begin(), models.end(), [&](const WhisperModel& other) {
            std::smatch otherMatch;
            return std::regex_match(other.name, otherMatch, largeVersion) &&
                   otherMatch[2].str() == match[2].str() && std::stoi(otherMatch[1].str()) > version;
        });
    };

    std::vector<std::size_t> main;
    for (std::size_t i = 0; i < models.size(); ++i) {
        const std::string& name = models[i].name;
        const bool variant = name.find("-q") != std::string::npos || name.find("-tdrz") != std::string::npos;
        const bool hasEnglishOnly = exists(name + ".en");
        if (models[i].installed || (!variant && !hasEnglishOnly && !superseded(name))) {
            main.push_back(i);
        }
    }
    return main;
}

std::string formatSize(std::uint64_t bytes) {
    constexpr double mib = 1024.0 * 1024.0;
    constexpr double gib = mib * 1024.0;
    std::array<char, 32> buffer{};
    if (static_cast<double>(bytes) >= gib) {
        std::snprintf(buffer.data(), buffer.size(), "%.1f GiB", static_cast<double>(bytes) / gib);
    } else {
        std::snprintf(buffer.data(), buffer.size(), "%.0f MiB", static_cast<double>(bytes) / mib);
    }
    return buffer.data();
}

std::string modelLabel(const WhisperModel& model) {
    if (model.installed) {
        return model.name + " ✓";
    }
    if (!model.sizeBytes) {
        return model.name + " (size unknown)";
    }
    return model.name + " (" + (model.sizeEstimated ? "~" : "") + formatSize(*model.sizeBytes) + ")";
}

std::optional<double> parseDownloadProgress(const std::string& line) {
    // wget and wget2: "ggml-base.bin   45%[=====>      ]  64.0M  10.2MB/s"
    static const std::regex percent(R"((\d{1,3})%)");
    // curl's meter: " 45  142M   45 64.0M    0     0  10.2M      0  0:00:13 ..."
    static const std::regex curl(R"(^\s*(\d{1,3})\s+[0-9.]+[kMGTP]?\s+\d{1,3}\s)");
    std::smatch match;
    if (std::regex_search(line, match, curl) || std::regex_search(line, match, percent)) {
        const int value = std::stoi(match[1].str());
        if (value >= 0 && value <= 100) {
            return value / 100.0;
        }
    }
    return std::nullopt;
}
