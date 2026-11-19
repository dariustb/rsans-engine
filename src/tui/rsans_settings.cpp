#include <rsans_settings.h>

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;
using json = nlohmann::json;

std::string defaultSettingsPath() {
    const char* config = std::getenv("XDG_CONFIG_HOME");
    if (config != nullptr && *config != '\0') {
        return (fs::path(config) / "rsans" / "settings.json").string();
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
        return (fs::path(home) / ".config" / "rsans" / "settings.json").string();
    }
    return "";
}

AppSettings loadSettings(const std::string& path) {
    AppSettings settings;
    std::ifstream in(path);
    if (!in) {
        return settings;
    }
    try {
        std::stringstream text;
        text << in.rdbuf();
        const json j = json::parse(text.str());
        if (j.contains("ffmpegPath") && j["ffmpegPath"].is_string()) {
            settings.ffmpegPath = j["ffmpegPath"].get<std::string>();
        }
        if (j.contains("whisperDir") && j["whisperDir"].is_string()) {
            settings.whisperDir = j["whisperDir"].get<std::string>();
        }
        if (j.contains("cmudictPath") && j["cmudictPath"].is_string()) {
            settings.cmudictPath = j["cmudictPath"].get<std::string>();
        }
    } catch (const std::exception&) {
        return AppSettings{};  // Damaged: start from the defaults
    }
    return settings;
}

std::string saveSettings(const std::string& path, const AppSettings& settings) {
    json j = json::object();
    if (!settings.ffmpegPath.empty()) {
        j["ffmpegPath"] = settings.ffmpegPath;
    }
    if (!settings.whisperDir.empty()) {
        j["whisperDir"] = settings.whisperDir;
    }
    if (!settings.cmudictPath.empty()) {
        j["cmudictPath"] = settings.cmudictPath;
    }

    std::error_code ec;
    const fs::path target(path);
    if (target.has_parent_path()) {
        fs::create_directories(target.parent_path(), ec);
    }
    // Written beside it and renamed, so it's never left half-written
    const fs::path temporary = target.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::trunc);
        out << j.dump(4) << '\n';
        out.close();
        if (!out) {
            fs::remove(temporary, ec);
            return "Can't save settings to " + path;
        }
    }
    fs::rename(temporary, target, ec);
    if (ec) {
        fs::remove(temporary, ec);
        return "Can't save settings to " + path + ": " + ec.message();
    }
    return "";
}
