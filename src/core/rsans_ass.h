#ifndef INCLUDED_RSANS_ASS
#define INCLUDED_RSANS_ASS

#include <rsans_data.h>

#include <string>

struct Ass {
    std::string text;

    Ass() = delete;
    Ass(const ProjectData& data);

  private:
    struct LineInfo {
        std::vector<std::string> lines;
        int minLineIdx;
        int maxLineIdx;
    };

    LineInfo buildLines(const std::vector<Token>& tokens);
    void buildScriptInfo(std::ostringstream& ss, const ProjectData& data);
    void buildStyles(std::ostringstream& ss, const ProjectData& data);
    void buildEvents(std::ostringstream& ss, const ProjectData& data, const LineInfo& lineInfo);
};

enum AssBorderStyle : int {
    Outline   = 1,
    OpaqueBox = 3
};

enum AssAlignment : int {
    BottomLeft     = 1,
    BottomCenter   = 2,
    BottomRight    = 3,
    MiddleLeft     = 4,
    MiddleCenter   = 5,
    MiddleRight    = 6,
    TopLeft        = 7,
    TopCenter      = 8,
    TopRight       = 9
};

struct AssStyle {
    // Required values
    std::string name;
    std::string fontName;
    int         fontSize;

    // Colors as ASS format
    std::string primaryColor   = "&H00FFFFFF"; // white
    std::string secondaryColor = "&H000000FF"; // blue (karaoke)
    std::string outlineColor   = "&H00000000"; // black
    std::string backColor      = "&H00000000"; // transparent by default

    // Font styles
    bool isBold      = false;
    bool isItalic    = false;
    bool isUnderline = false;
    bool isStrikeOut = false;

    // Transform
    int  scaleX    = 100;
    int  scaleY    = 100;
    int  spacing   = 0;
    int  angle     = 0;

    // Border / shadow
    AssBorderStyle borderStyle = AssBorderStyle::Outline;
    int  outlineWidth          = 2;
    int  shadowDepth           = 0;

    // Alignment and margins
    AssAlignment alignment = AssAlignment::TopLeft;
    int marginL            = 10;
    int marginR            = 10;
    int marginV            = 10;

    int encoding           = 1; // usually 1 (ANSI) or 0, depending on your use

    std::string toAssLine() const;

    AssStyle() = delete;
    AssStyle(const std::string styleName, const std::string fontName, const int fontSize);
};

#endif
