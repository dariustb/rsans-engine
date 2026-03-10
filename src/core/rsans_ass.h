#ifndef INCLUDED_RSANS_ASS
#define INCLUDED_RSANS_ASS

#include <rsans_data.h>

#include <ass/ass.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

class Ass {
  private:
    enum Layer: int;
    enum BorderStyle: int;
    enum Alignment: int;
    enum TextWrap : int;

    struct Style;
    struct Dialogue;
    struct LineInfo;

    const ProjectData& d_data;

    std::unique_ptr<LineInfo> d_lineInfo_p;

    struct LibraryDeleter {
        void operator()(ASS_Library* p) const { ass_library_done(p); }
    };
    struct RendererDeleter {
        void operator()(ASS_Renderer* p) const { ass_renderer_done(p); }
    };
    struct TrackDeleter {
        void operator()(ASS_Track* p) const { ass_free_track(p); }
    };

    std::unique_ptr<ASS_Library, LibraryDeleter> d_library_p;
    std::unique_ptr<ASS_Renderer, RendererDeleter> d_renderer_p;

    int d_fontHeight;
    double d_topMargin;
    double d_leftMargin;

    std::ostringstream d_ss;
    
    int renderAssHeight(const std::string& styleName);
    int renderAssWidth(const std::string& text, const std::string& styleName);
    int getStringWidth(const std::string& text);

    void buildScriptInfo(std::ostringstream& ss);
    void buildStyleInfo(std::ostringstream& ss);
    void buildStyles(std::ostringstream& ss);
    void buildEventInfo(std::ostringstream& ss);
    void buildHeaderLabel(std::ostringstream& ss);
    void buildEvents(std::ostringstream& ss);
    void buildHighlights(std::ostringstream& ss);

  public:
    std::string text() const;

    Ass() = delete;
    Ass(const ProjectData& data);
};

enum Ass::Layer : int {
    LyricHighlight,
    LyricText,
    HeaderBackground,
    HeaderText
};

enum Ass::BorderStyle : int {
    Outline   = 1,
    OpaqueBox = 3
};

enum Ass::Alignment : int {
    BottomLeft   = 1,
    BottomCenter = 2,
    BottomRight  = 3,
    MiddleLeft   = 4,
    MiddleCenter = 5,
    MiddleRight  = 6,
    TopLeft      = 7,
    TopCenter    = 8,
    TopRight     = 9
};

enum Ass::TextWrap : int {
    SmartWrap = 0, // default
    EolWrap = 1,   // only breaks at \N escapes
    NoWrap = 2,
    SmartWrapTopHeavy = 3
};

struct Ass::LineInfo {
    std::vector<std::string> lines;
    std::vector<size_t> tokenCharPositions;  // char position of each token within its line, parallel to tokens
    int minLineIdx;
    int maxLineIdx;

    LineInfo() = delete;
    LineInfo(const std::vector<Token>& tokens);
};

struct Ass::Style {
    std::string name;
    std::string fontName;
    int         fontSize;

    std::string primaryColor   = "&H00000000";  // black
    std::string secondaryColor = "&H00000000";
    std::string outlineColor   = "&H00000000";
    std::string backColor      = "&H00000000";  // transparent by default

    bool isBold      = true;
    bool isItalic    = false;
    bool isUnderline = false;
    bool isStrikeOut = false;

    int  scaleX  = 100;
    int  scaleY  = 100;
    int  spacing = 0;
    int  angle   = 0;

    BorderStyle borderStyle = BorderStyle::Outline;
    int  outlineWidth = 0;
    int  shadowDepth  = 0;

    Alignment alignment;
    int marginL  = 10;
    int marginR  = 10;
    int marginV  = 10;
    int encoding = 1;  // usually 1 (ANSI) or 0

    Style() = delete;
    Style(const std::string styleName, const std::string fontName, const int fontSize, const Alignment alignment);
    Style(const std::string styleName, const std::string fontName, const int fontSize, const Alignment alignment,
          const std::string& primaryColor, const BorderStyle borderStyle);

    std::string toAssLine() const;
};

struct Ass::Dialogue {
    // Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
    int layer;
    int64_t startTime;  // start time in milliseconds, TODO: can we do seconds instead?
    int64_t endTime;    // end time in milliseconds
    std::string styleName;
    std::string text;   // raw ASS text

    // Non-essentials
    std::string name;    // name of character giving dialogue, usually empty
    int marginL = 0, marginR = 0, marginV = 0; // posX/posY overrides margins, keep at 0
    std::string effect;  // usually empty

    // Overrides
    int posX;
    int posY;
    TextWrap textWrap = NoWrap;

    // Box-drawing Overrides
    int rectangleWidth  = 0;
    int rectangleHeight = 0;

    Dialogue() = delete;
    Dialogue(const std::string& styleName, const int layer, const int64_t startTime, const int64_t endTime,
        const std::string text, const int posX, const int posY);
    Dialogue(const std::string& styleName, const int layer, const int64_t startTime, const int64_t endTime,
        const int posX, const int posY, const int width, const int height); // Constructor for header background dialogue
    
    std::string toAssLine() const;
};

#endif
