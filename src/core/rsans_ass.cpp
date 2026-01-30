#include <rsans_ass.h>

#include <rsans_data.h>
#include <rsans_font.h>

#include <ass/ass.h>

#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void silentAssLog(int, const char*, va_list, void*) {}

// TODO: use id to index color in json and create a color swatch
std::string hexToAssColor(const std::string& hexColor) {
    const std::string hex = hexColor.substr(1);
    const std::string r = hex.substr(0, 2);
    const std::string g = hex.substr(2, 2);
    const std::string b = hex.substr(4, 2);
    return "&H00" + b + g + r;
}

std::string formatTime(int ms) {
    const int cs = (ms / 10) % 100;
    const int sec = (ms / 1000) % 60;
    const int min = (ms / 60000) % 60;
    const int hour = ms / 3600000;

    std::ostringstream oss;
    oss << hour << ":"
        << (min < 10 ? "0" : "") << min << ":"
        << (sec < 10 ? "0" : "") << sec << "."
        << (cs < 10 ? "0" : "") << cs;
    return oss.str();
}

}

Ass::Ass(const ProjectData& data)
    : d_assLibrary(nullptr)
    , d_assRenderer(nullptr)
    , d_fontFamily(data.layout.fontName)
    , d_fontSize(data.layout.fontSize)
{
    d_assLibrary = ass_library_init();
    if (!d_assLibrary) {
        throw std::runtime_error("Failed to initialize libass library");
    }
    ass_set_message_cb(d_assLibrary, silentAssLog, nullptr);

    d_assRenderer = ass_renderer_init(d_assLibrary);
    if (!d_assRenderer) {
        ass_library_done(d_assLibrary);
        throw std::runtime_error("Failed to initialize libass renderer");
    }

    ass_set_storage_size(d_assRenderer, data.video.width, data.video.height);
    ass_set_frame_size(d_assRenderer, data.video.width, data.video.height);
    ass_set_fonts(d_assRenderer, data.layout.fontPath.c_str(),
                  d_fontFamily.c_str(), ASS_FONTPROVIDER_NONE, nullptr, 0);

    std::ostringstream ss;

    const LineInfo lineInfo = buildLines(data.tokens);
    buildScriptInfo(ss, data);
    buildStyleInfo(ss, data);
    buildStyles(ss, data);
    buildEventInfo(ss, data);
    buildEvents(ss, data, lineInfo);
    buildHighlights(ss, data, lineInfo);


    text = ss.str();
}

Ass::~Ass() {
    if (d_assRenderer) {
        ass_renderer_done(d_assRenderer);
    }
    if (d_assLibrary) {
        ass_library_done(d_assLibrary);
    }
}

Ass::LineInfo Ass::buildLines(const std::vector<Token>& tokens) {
    LineInfo info;
    info.minLineIdx = -1;
    info.maxLineIdx = -1;

    std::string tokenLineStr;
    int lastLineIdx = -1;

    for (const Token& token : tokens) {
        if (info.minLineIdx == -1 || token.lineIndex < info.minLineIdx) {
            info.minLineIdx = token.lineIndex;
        }
        if (info.maxLineIdx == -1 || token.lineIndex > info.maxLineIdx) {
            info.maxLineIdx = token.lineIndex;
        }

        if (token.lineIndex != lastLineIdx) {
            if (!tokenLineStr.empty()) {
                info.lines.push_back(std::move(tokenLineStr));
            }
            tokenLineStr = token.text;
            lastLineIdx = token.lineIndex;
        } else {
            tokenLineStr += ' ';
            tokenLineStr += token.text;
        }
    }

    if (!tokenLineStr.empty()) {
        info.lines.push_back(std::move(tokenLineStr));
    }

    return info;
}

void Ass::buildScriptInfo(std::ostringstream& ss, const ProjectData& data) {
    ss << "[Script Info]\n";
    ss << "Title: RSANS Generated ASS\n";
    ss << "ScriptType: v4.00+\n";
    ss << "PlayResX: " << data.video.width << "\n";
    ss << "PlayResY: " << data.video.height << "\n\n";
}

void Ass::buildStyleInfo(std::ostringstream& ss, const ProjectData& data) {
    ss << "[V4+ Styles]\n";
    ss << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
       << "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
       << "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
       << "Alignment, MarginL, MarginR, MarginV, Encoding\n";
}

void Ass::buildStyles(std::ostringstream& ss, const ProjectData& data) {
    // Make Base style line
    const AssStyle baseStyle("Base", data.layout.fontName, data.layout.fontSize);
    ss << baseStyle.toAssLine();

    // Make Highlight style lines
    AssStyle highlightStyle = baseStyle;
    for (const auto& [groupName, style] : data.rhymeStyles) {
        highlightStyle.name = groupName;
        highlightStyle.primaryColor = hexToAssColor(style.color);
        highlightStyle.borderStyle = AssBorderStyle::OpaqueBox;
        ss << highlightStyle.toAssLine();
    }
    ss << "\n";
}

void Ass::buildEventInfo(std::ostringstream& ss, const ProjectData& data) {
    ss << "[Events]\n";
    ss << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
}

void Ass::buildEvents(std::ostringstream& ss, const ProjectData& data, const LineInfo& lineInfo) {
    const double leftMargin = 50.0;
    const double topMargin = 50.0;
    const int audioLengthMs = data.audio.length * 1000;

    FTFont font(data.layout.fontPath, data.layout.fontSize);
    const int fontHeight = font.getFontPixelHeight();

    // Build base text as separate dialogues per line with explicit positioning
    for (size_t i = 0; i < lineInfo.lines.size(); ++i) {
        const double lineY = topMargin + i * fontHeight;
        ss << "Dialogue: 1," << formatTime(0) << "," << formatTime(audioLengthMs)
           << ",Base,,0,0,0,,{\\an7\\pos(" << leftMargin << "," << lineY << ")\\q2}"
           << lineInfo.lines[i] << "\n";
    }
}

void Ass::buildHighlights(std::ostringstream& ss, const ProjectData& data, const LineInfo& lineInfo) {
    const double leftMargin = 50.0;
    const double topMargin = 50.0;
    const int audioLengthMs = data.audio.length * 1000;

    FTFont font(data.layout.fontPath, data.layout.fontSize);
    const int fontLineHeight = font.getFontPixelHeight();

    for (const Token& token : data.tokens) {
        if (!token.rhymeGroup.has_value()) {
            continue;
        }

        const std::string& groupName = token.rhymeGroup.value();
        const auto styleIt = data.rhymeStyles.find(groupName);
        if (styleIt == data.rhymeStyles.end()) {
            continue;
        }

        const std::string& lineText = lineInfo.lines.at(token.lineIndex - lineInfo.minLineIdx);
        // FIXME: This will only find the first occurence of the word
        // May be bad if the word is used multiple times in a line
        const size_t tokenCharPos = lineText.find(token.text);

        const std::string textBeforeToken = lineText.substr(0, tokenCharPos);
        const double tokenX = leftMargin + getStringWidth(textBeforeToken, data);
        const double lineY = topMargin + (token.lineIndex - lineInfo.minLineIdx) * fontLineHeight;

        // Use transparent text with colored border to emulate highlight
        // \1a&HFF& = fully transparent text
        // \3c = outline/border color (used as highlight color)
        // \3a&H00& = fully opaque border
        // \bord = border thickness for coverage
        const std::string assColor = hexToAssColor(styleIt->second.color);
        const int borderSize = 1;//fontHeight / 2;

        ss << "Dialogue: 0," << formatTime(token.startMs) << ","
           << formatTime(audioLengthMs) << "," << groupName << ",,0,0,0,,"
           << "{\\an7\\pos(" << tokenX << "," << lineY << ")"
           << "\\1a&HFF&\\3c" << assColor << "\\3a&H00&"
           << "\\bord" << borderSize << "\\shad0}"
           << token.text << "\n";
    }
}

int Ass::renderAssWidth(const std::string& text, const ProjectData& data) {
    // TODO: refactor other script setup functions and call them here instead
    std::ostringstream oss;

    buildScriptInfo(oss, data);
    buildStyleInfo(oss, data);
    buildStyles(oss, data);
    buildEventInfo(oss, data);
    
    const std::string textDialogue =
        "Dialogue: 0,0:00:00.00,0:00:10.00,Base,,0,0,0,,{\\an7\\pos(0,0)}" + text + "\n";
    oss << textDialogue;

    ASS_Track* track = ass_read_memory(
        d_assLibrary,
        const_cast<char*>(oss.str().c_str()),
        oss.str().size(),
        nullptr);

    if (!track) {
        return 0;
    }

    int detect_change = 0;
    ASS_Image* img = ass_render_frame(d_assRenderer, track, 0, &detect_change);

    int maxX = 0;
    for (ASS_Image* cur = img; cur; cur = cur->next) {
        if (cur->w > 0 && cur->h > 0) {
            int right = cur->dst_x + cur->w;
            if (right > maxX) {
                maxX = right;
            }
        }
    }

    ass_free_track(track);
    return maxX;
}

int Ass::getStringWidth(const std::string& text, const ProjectData& data) {
    const std::string sentinel = "|";  // Trailing spaces get removed in libass render
    const int combined = renderAssWidth(text + sentinel, data);
    const int sentinelWidth = renderAssWidth(sentinel, data);
    return combined - sentinelWidth;
}

AssStyle::AssStyle(const std::string styleName, const std::string fontName, const int fontSize)
: name(styleName)
, fontName(fontName)
, fontSize(fontSize)
{}

std::string AssStyle::toAssLine() const {
    std::ostringstream oss;
    oss << "Style: " << name << ',' << fontName << ',' << fontSize << ','
        << primaryColor << ',' << secondaryColor << ',' << outlineColor << ',' << backColor << ','
        << (isBold ? -1 : 0) << ',' << (isItalic ? -1 : 0) << ',' // ASS: -1 = true, 0 = false
        << (isUnderline ? -1 : 0) << ',' << (isStrikeOut ? -1 : 0) << ','
        << scaleX << ',' << scaleY << ','
        << spacing << ',' << angle << ','
        << static_cast<int>(borderStyle) << ','
        << outlineWidth << ',' << shadowDepth  << ','
        << static_cast<int>(alignment) << ','
        << marginL << ',' << marginR << ',' << marginV << ','
        << encoding << '\n';
    return oss.str();
}

AssDialogue::AssDialogue(int64_t startMs, int64_t endMs, std::string styleName, std::string text_)
: startMs(startMs)
, endMs(endMs)
, style(std::move(styleName))
, text(std::move(text_))
{}

std::string AssDialogue::toAssLine() const {
    std::ostringstream oss;
    oss << "Dialogue: " << layer << ','
        << formatTime(startMs) << ',' << formatTime(endMs) << ','
        << style << ',' << name << ','
        << marginL << ',' << marginR << ',' << marginV << ','
        << effect << ',' << text;

    return oss.str();
}
