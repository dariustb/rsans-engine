#ifndef INCLUDED_RSANS_DATA
#define INCLUDED_RSANS_DATA

#include <optional>
#include <string>
#include <vector>

struct Token {
    int id;
    std::string text;
    int startMs;
    int endMs;
    int lineIndex;
    std::optional<int> rhymeIndex;

    Token() = delete;
    Token(
        int id,
        const std::string& text,
        int startMs,
        int endMs,
        int lineIndex,
        const std::optional<int>& rhymeIndex
    );
};

class Color {
  private:
    std::string d_hex;
    std::string d_ass;

    std::string toAss(const std::string& hexValue);
  public:
    std::string hex() const;
    std::string ass() const;

    Color() = delete;
    Color(const std::string& hexValue);
};

struct ProjectData {
    struct HeaderData {
        std::string media;
    } header;

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
        std::string fontPath;
        int fontSize;
        int lineHeight;
    } layout;

    struct ModelPaths {
        // TODO: Add tiny/small/med/lg/turbo model support
        std::string base;
    } model;

    std::string cmudict;

    std::vector<Token> tokens;

    std::vector<Color> colorSwatch;

    ProjectData() = delete;
    ProjectData(const std::string& jsonContent);
    ProjectData(ProjectData base, std::vector<Token>& newTokens);
    ProjectData(ProjectData&& other) noexcept;

    std::string toJson() const;
};

#endif
