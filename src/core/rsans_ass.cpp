#include <rsans_ass.h>

#include <rsans_data.h>

#include <ass/ass.h>

#include <memory>
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
    : d_data(data)
    , d_lineInfo_p(std::make_unique<LineInfo>(data.tokens))
    , d_fontHeight(0)
{
    // Init libass stuff
    d_library_p.reset(ass_library_init());
    if (!d_library_p) {
        throw std::runtime_error("Failed to initialize libass library");
    }
    ass_set_message_cb(d_library_p.get(), silentAssLog, nullptr);

    d_renderer_p.reset(ass_renderer_init(d_library_p.get()));
    if (!d_renderer_p) {
        throw std::runtime_error("Failed to initialize libass renderer");
    }
    ass_set_storage_size(d_renderer_p.get(), data.video.width, data.video.height);
    ass_set_frame_size(d_renderer_p.get(), data.video.width, data.video.height);
    ass_set_fonts(d_renderer_p.get(), data.layout.fontPath.c_str(),
    data.layout.fontName.c_str(), ASS_FONTPROVIDER_NONE, nullptr, 0);

    d_fontHeight = renderAssHeight();

    // Set up ASS file stuff
    buildScriptInfo(d_ss);
    buildStyleInfo(d_ss);
    buildStyles(d_ss);
    buildEventInfo(d_ss);
    buildEvents(d_ss);
    buildHighlights(d_ss);
}

std::string Ass::text() const {
    return d_ss.str();
}

int Ass::renderAssHeight() {
    std::ostringstream oss;

    buildScriptInfo(oss);
    buildStyleInfo(oss);
    buildStyles(oss);
    buildEventInfo(oss);

    const std::string text = "Hg";  // chars uses the top and bottom of the glyph space
    const std::string textDialogue =
        "Dialogue: 0,0:00:00.00,0:00:10.00,Base,,0,0,0,,{\\an7\\pos(0,0)}" + text + "\n";
    oss << textDialogue;

    std::unique_ptr<ASS_Track, TrackDeleter> track(ass_read_memory(
        d_library_p.get(),
        const_cast<char*>(oss.str().c_str()),
        oss.str().size(),
        nullptr));

    if (!track) {
        return 0;
    }

    int detect_change = 0;
    ASS_Image* img = ass_render_frame(d_renderer_p.get(), track.get(), 0, &detect_change);

    int maxY = 0;
    for (ASS_Image* cur = img; cur; cur = cur->next) {
        if (cur->w > 0 && cur->h > 0) {
            int bottom = cur->dst_y + cur->h;
            if (bottom > maxY) {
                maxY = bottom;
            }
        }
    }

    return maxY;
}

int Ass::renderAssWidth(const std::string& text) {
    std::ostringstream oss;

    buildScriptInfo(oss);
    buildStyleInfo(oss);
    buildStyles(oss);
    buildEventInfo(oss);
    
    const std::string textDialogue =
        "Dialogue: 0,0:00:00.00,0:00:10.00,Base,,0,0,0,,{\\an7\\pos(0,0)}" + text + "\n";
    oss << textDialogue;

    std::unique_ptr<ASS_Track, TrackDeleter> track(ass_read_memory(
        d_library_p.get(),
        const_cast<char*>(oss.str().c_str()),
        oss.str().size(),
        nullptr));

    if (!track) {
        return 0;
    }

    int detect_change = 0;
    ASS_Image* img = ass_render_frame(d_renderer_p.get(), track.get(), 0, &detect_change);

    int maxX = 0;
    for (ASS_Image* cur = img; cur; cur = cur->next) {
        if (cur->w > 0 && cur->h > 0) {
            int right = cur->dst_x + cur->w;
            if (right > maxX) {
                maxX = right;
            }
        }
    }

    return maxX;
}

int Ass::getStringWidth(const std::string& text) {
    const std::string sentinel = "|";  // Trailing spaces get removed in libass render
    const int combined = renderAssWidth(text + sentinel);
    const int sentinelWidth = renderAssWidth(sentinel);
    return combined - sentinelWidth;
}

void Ass::buildScriptInfo(std::ostringstream& ss) {
    ss << "[Script Info]\n";
    ss << "Title: RSANS Generated ASS\n";
    ss << "ScriptType: v4.00+\n";
    ss << "PlayResX: " << d_data.video.width << "\n";
    ss << "PlayResY: " << d_data.video.height << "\n\n";
}

void Ass::buildStyleInfo(std::ostringstream& ss) {
    ss << "[V4+ Styles]\n";
    ss << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
       << "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
       << "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
       << "Alignment, MarginL, MarginR, MarginV, Encoding\n";
}

void Ass::buildStyles(std::ostringstream& ss) {
    // Make Base style line
    const Style baseStyle("Base", d_data.layout.fontName, d_data.layout.fontSize);
    ss << baseStyle.toAssLine();

    // Make Highlight style lines
    Style highlightStyle = baseStyle;
    for (const auto& [groupName, style] : d_data.rhymeStyles) {
        highlightStyle.name = groupName;
        highlightStyle.primaryColor = hexToAssColor(style.color);
        highlightStyle.borderStyle = BorderStyle::OpaqueBox;
        ss << highlightStyle.toAssLine();
    }
    ss << "\n";
}

void Ass::buildEventInfo(std::ostringstream& ss) {
    ss << "[Events]\n";
    ss << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
}

void Ass::buildEvents(std::ostringstream& ss) {
    const double leftMargin = 50.0;
    const double topMargin = 50.0;
    const int audioLengthMs = d_data.audio.length * 1000;

    // Build base text as separate dialogues per line with explicit positioning
    for (size_t i = 0; i < d_lineInfo_p->lines.size(); ++i) {
        const double lineY = topMargin + i * d_fontHeight;
        ss << "Dialogue: 1," << formatTime(0) << "," << formatTime(audioLengthMs)
           << ",Base,,0,0,0,,{\\an7\\pos(" << leftMargin << "," << lineY << ")\\q2}"
           << d_lineInfo_p->lines[i] << "\n";
    }
}

void Ass::buildHighlights(std::ostringstream& ss) {
    const double leftMargin = 50.0;
    const double topMargin = 50.0;
    const int audioLengthMs = d_data.audio.length * 1000;

    for (const Token& token : d_data.tokens) {
        if (!token.rhymeGroup.has_value()) {
            continue;
        }

        const std::string& groupName = token.rhymeGroup.value();
        const auto styleIt = d_data.rhymeStyles.find(groupName);
        if (styleIt == d_data.rhymeStyles.end()) {
            continue;
        }

        const std::string& lineText = d_lineInfo_p->lines.at(token.lineIndex - d_lineInfo_p->minLineIdx);
        // FIXME: This will only find the first occurence of the word
        // May be bad if the word is used multiple times in a line
        const size_t tokenCharPos = lineText.find(token.text);

        const std::string textBeforeToken = lineText.substr(0, tokenCharPos);
        const double tokenX = leftMargin + getStringWidth(textBeforeToken);
        const double lineY = topMargin + (token.lineIndex - d_lineInfo_p->minLineIdx) * d_fontHeight;

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

Ass::Style::Style(const std::string styleName, const std::string fontName, const int fontSize)
: name(styleName)
, fontName(fontName)
, fontSize(fontSize)
{}

std::string Ass::Style::toAssLine() const {
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

Ass::Dialogue::Dialogue(int64_t startMs, int64_t endMs, std::string styleName, std::string text_)
: startMs(startMs)
, endMs(endMs)
, style(std::move(styleName))
, text(std::move(text_))
{}

std::string Ass::Dialogue::toAssLine() const {
    std::ostringstream oss;
    oss << "Dialogue: " << layer << ','
        << formatTime(startMs) << ',' << formatTime(endMs) << ','
        << style << ',' << name << ','
        << marginL << ',' << marginR << ',' << marginV << ','
        << effect << ',' << text << '\n';

    return oss.str();
}

Ass::LineInfo::LineInfo(const std::vector<Token>& tokens) {
    minLineIdx = -1;
    maxLineIdx = -1;

    std::string tokenLineStr;
    int lastLineIdx = -1;

    for (const Token& token : tokens) {
        if (minLineIdx == -1 || token.lineIndex < minLineIdx) {
            minLineIdx = token.lineIndex;
        }
        if (maxLineIdx == -1 || token.lineIndex > maxLineIdx) {
            maxLineIdx = token.lineIndex;
        }

        if (token.lineIndex != lastLineIdx) {
            if (!tokenLineStr.empty()) {
                lines.push_back(std::move(tokenLineStr));
            }
            tokenLineStr = token.text;
            lastLineIdx = token.lineIndex;
        } else {
            // Put commas next to end of text for grammar
            if (token.text != ",") {
                tokenLineStr += ' ';
            }
            tokenLineStr += token.text;
        }
    }

    if (!tokenLineStr.empty()) {
        lines.push_back(std::move(tokenLineStr));
    }
}
