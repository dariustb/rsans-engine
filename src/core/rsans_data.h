#ifndef INCLUDED_RSANS_DATA
#define INCLUDED_RSANS_DATA

#include <map>
#include <optional>
#include <string>
#include <vector>

struct Token {
    int id;
    std::string text;
    int startMs;
    int endMs;
    int lineIndex;
    std::optional<std::string> rhymeGroup;
};

struct ProjectData {
    struct AudioData {
        std::string path;
        double length;
    } audio;

    struct VideoConfig {
        int width;
        int height;
        std::string background;
    } video;
    
    struct LayoutConfig {
        std::string fontName;
        int fontSize;
        int lineHeight;
    } layout;
    
    // TODO: Get rid of it!
    struct RhymeStyle {
        std::string color;
    };

    std::map<std::string, RhymeStyle> rhymeStyles;
    std::vector<Token> tokens;

    ProjectData() = delete;
    ProjectData(const std::string& jsonContent);
    ProjectData(ProjectData base, std::vector<Token>& newTokens);
    ProjectData(ProjectData&& other) noexcept;
};

#endif
