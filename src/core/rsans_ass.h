#ifndef INCLUDED_RSANS_ASS
#define INCLUDED_RSANS_ASS

#include <rsans_data.h>

#include <ass/ass.h>

#include <sstream>
#include <string>
#include <vector>

struct Ass {
    std::string text;

    Ass() = delete;
    Ass(const Ass&) = delete;
    Ass& operator=(const Ass&) = delete;
    Ass(const ProjectData& data);
    ~Ass();

  private:
    struct LineInfo {
        std::vector<std::string> lines;
        int minLineIdx;
        int maxLineIdx;

        LineInfo() = delete;
        LineInfo(const std::vector<Token>& tokens);
    } const d_lineInfo;
    const ProjectData& d_data;
    ASS_Library*  d_assLibrary;
    ASS_Renderer* d_assRenderer;
    
    int renderAssWidth(const std::string& text);
    int getStringWidth(const std::string& text);
    // int getFontHeight();

    void buildScriptInfo(std::ostringstream& ss);
    void buildStyleInfo(std::ostringstream& ss);
    void buildStyles(std::ostringstream& ss);
    void buildEventInfo(std::ostringstream& ss);
    void buildEvents(std::ostringstream& ss);
    void buildHighlights(std::ostringstream& ss);
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
    std::string primaryColor   = "&H00000000";  // black
    std::string secondaryColor = "&H00000000";
    std::string outlineColor   = "&H00000000";
    std::string backColor      = "&H00000000";  // transparent by default

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
    int  outlineWidth          = 0;
    int  shadowDepth           = 0;

    // Alignment and margins
    AssAlignment alignment = AssAlignment::TopLeft;
    int marginL            = 10;
    int marginR            = 10;
    int marginV            = 10;

    int encoding           = 1;  // usually 1 (ANSI) or 0

    std::string toAssLine() const;

    AssStyle() = delete;
    AssStyle(const std::string styleName, const std::string fontName, const int fontSize);
};

struct AssDialogue {
    int      layer   = 0;
    int64_t  startMs = 0;  // start time in milliseconds
    int64_t  endMs   = 0;  // end time in milliseconds

    std::string style;
    std::string name;  // usually empty

    int marginL      = 0;  // in script units, zero-padded to 4 digits
    int marginR      = 0;
    int marginV      = 0;

    std::string effect;  // usually empty
    std::string text;    // raw ASS text, including any override tags

    AssDialogue() = delete;
    AssDialogue(int64_t startMs, int64_t endMs, std::string styleName, std::string text_);

    std::string toAssLine() const;
};


#endif
