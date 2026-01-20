#ifndef INCLUDED_RSANS_ASS
#define INCLUDED_RSANS_ASS

#include <rsans_data.h>

#include <string>

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

std::string generateAss(const ProjectData& data);

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
