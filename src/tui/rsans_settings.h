#ifndef INCLUDED_RSANS_SETTINGS
#define INCLUDED_RSANS_SETTINGS

#include <string>

// Settings that belong to this computer rather than a project, remembered
// between runs. An empty value means "not set" (use the default).
struct AppSettings {
    std::string ffmpegPath;
    std::string whisperDir;
};

// Where settings are kept: $XDG_CONFIG_HOME/rsans/settings.json, else
// ~/.config/rsans/settings.json. Empty if neither variable is set.
std::string defaultSettingsPath();

// Reads settings; a missing or unreadable file gives empty settings
AppSettings loadSettings(const std::string& path);

// Writes settings, creating the folder if needed. Returns "" or why it failed.
std::string saveSettings(const std::string& path, const AppSettings& settings);

#endif
