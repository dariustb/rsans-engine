#ifndef INCLUDED_RSANS_ASS
#define INCLUDED_RSANS_ASS

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

// TODO: Move ProjectData into its own files. It's used in ASS and in export.
struct ProjectData {
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

    // VideoConfig video;
    // LayoutConfig layout;
    std::map<std::string, RhymeStyle> rhymeStyles;
    
    std::vector<Token> tokens;
    std::string audioPath;

    double audioLength;

    ProjectData() = delete;
    ProjectData(const std::string& jsonContent);
};

// TODO: I don't think we need this
struct TokenWithOffset {
    Token token;
    int columnOffset; // This can be derived: token.text.length()
    double x;
    double y;
};

struct LineData {
    std::string lineText;
    std::vector<TokenWithOffset> tokens;
    double lineStartX;
    double lineY;
};

std::string generateAss(const std::string& jsonContent);

// Helper functions for generateAss
std::map<int, std::vector<Token>> groupTokensByLine(
    const std::vector<Token>& tokens);
std::map<int, LineData> processLines(
    const std::map<int, std::vector<Token>>& lineTokens,
    const ProjectData::VideoConfig& video, const ProjectData::LayoutConfig& layout, int& maxEndMs);
std::string hexToAssColor(const std::string& hexColor);
std::string formatTime(int ms);
std::string buildScriptInfo(int playResX, int playResY);
std::string buildStyles(const std::string& fontName, int fontSize,
                        const std::map<std::string, ProjectData::RhymeStyle>& rhymeStyles);
std::string buildEvents(const std::map<int, LineData>& linesData, double centerX,
                        double centerY, int maxEndMs, int fontSize);

#endif
