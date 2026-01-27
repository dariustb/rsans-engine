#include <rsans_ass.h>

#include <rsans_data.h>

#include <sstream>

namespace {

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

Ass::Ass(const ProjectData& data) {
    std::ostringstream ss;

    const LineInfo lineInfo = buildLines(data.tokens);
    buildScriptInfo(ss, data);
    buildStyles(ss, data);
    buildEvents(ss, data, lineInfo);

    text = ss.str();
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

void Ass::buildStyles(std::ostringstream& ss, const ProjectData& data) {
    ss << "[V4+ Styles]\n";
    ss << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
       << "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
       << "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
       << "Alignment, MarginL, MarginR, MarginV, Encoding\n";

    ss << "Style: Base," << data.layout.fontName << "," << data.layout.fontSize
       << ",&H00606060,&H000000FF,&H00000000,&H0000FF00,0,0,0,0,100,100,0,0,3,5,0,7,0,0,0,1\n";

    for (const auto& [group, style] : data.rhymeStyles) {
        const std::string assColor = hexToAssColor(style.color);
        ss << "Style: " << group << "," << data.layout.fontName << "," << data.layout.fontSize
           << "," << assColor << ",&H000000FF,&H00000000,&H00000000"
           << ",0,0,0,0,100,100,0,0,1,0,0,7,0,0,0,1\n";
    }
    ss << "\n";
}

void Ass::buildEvents(std::ostringstream& ss, const ProjectData& data, const LineInfo& lineInfo) {
    ss << "[Events]\n";
    ss << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";

    const double leftMargin = 50.0;
    const double topMargin = 50.0;
    const double cellWidth = 30;

    // Build base text from lines
    std::string baseText;
    for (const std::string& line : lineInfo.lines) {
        if (!baseText.empty()) {
            baseText += "\\N";
        }
        baseText += line;
    }

    const int audioLengthMs = data.audio.length * 1000;
    ss << "Dialogue: 1," << formatTime(0) << "," << formatTime(audioLengthMs)
       << ",Base,,0,0,0,,{\\an7\\pos(" << leftMargin << "," << topMargin << ")}"
       << baseText << "\n";

    // Create rhyme highlight dialogues
    for (const Token& token : data.tokens) {
        if (token.rhymeGroup.has_value()) {
            const std::string& lineText = lineInfo.lines[token.lineIndex - lineInfo.minLineIdx];
            // FIXME: This will only find the first occurence of the word
            // May be bad if the word is used multiple times in a line
            const size_t tokenCharPos = lineText.find(token.text);

            const double tokenX = leftMargin + (tokenCharPos * cellWidth);
            const double boxWidth = token.text.length() * cellWidth;
            const double boxHeight = data.layout.fontSize + 12;

            const double lineY = topMargin + (token.lineIndex - lineInfo.minLineIdx) * data.layout.lineHeight;
            const double boxTop = lineY - (boxHeight - data.layout.fontSize) / 2.0;

            ss << "Dialogue: 0," << formatTime(token.startMs) << ","
               << formatTime(audioLengthMs) << ","
               << token.rhymeGroup.value() << ",,0,0,0,,{\\an7\\pos("
               << tokenX << "," << boxTop << ")\\p1}"
               << "m 0 0 l " << boxWidth << " 0 " << boxWidth << " " << boxHeight
               << " 0 " << boxHeight << "{\\p0}\n";
        }
    }
}
