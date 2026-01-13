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

// TODO: Move below structs back later to _ass.h

// TODO: I don't think we need this
struct TokenWithOffset {
    Token token;
    int columnOffset; // This can be derived: token.text.length()
    double x;
    double y;
};

// TODO: This can be contained into ASS struct constructor; it's only needed there
struct LineData {
    std::string lineText;
    std::vector<TokenWithOffset> tokens;
    double lineStartX;
    double lineY;
};

#endif
