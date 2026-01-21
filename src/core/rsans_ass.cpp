#include <rsans_ass.h>

#include <rsans_data.h>

#include <sstream>

namespace {

// TODO: Remove Hex values from JSON map,
// just use id to index and create a color swatch
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
    // Populate lines and create mapping from lineIndex to lines position
    std::map<int, int> lineIndexToPos;
    std::string tokenLineStr;
    int lastLineIdx = -1;
    int minLineIdx = -1;
    int maxLineIdx = -1;

    std::vector<std::string> lines;
    for (const Token& token : data.tokens) {
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
            lineIndexToPos[token.lineIndex] = lines.size();
            tokenLineStr = token.text;
            lastLineIdx = token.lineIndex;
        } else {
            tokenLineStr += ' ';
            tokenLineStr += token.text;
        }
    }
    // Push the last line
    if (!tokenLineStr.empty()) {
        lines.push_back(std::move(tokenLineStr));
    }

    std::ostringstream ss;
    // Script Info
    ss << "[Script Info]\n";
    ss << "Title: RSANS Generated ASS\n";
    ss << "ScriptType: v4.00+\n";
    ss << "PlayResX: " << data.video.width << "\n";
    ss << "PlayResY: " << data.video.height << "\n\n";

    // Styles
    ss << "[V4+ Styles]\n";
    ss << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
       << "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
       << "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
       << "Alignment, MarginL, MarginR, MarginV, Encoding\n";

    ss << "Style: Base," << data.layout.fontName << "," << data.layout.fontSize
       << ",&H00606060,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,0,0,4,0,0,0,1\n";

    for (const auto& [group, style] : data.rhymeStyles) {
        const std::string assColor = hexToAssColor(style.color);
        ss << "Style: Rhyme_" << group << "," << data.layout.fontName << "," << data.layout.fontSize
           << "," << assColor << ",&H000000FF,&H00000000,&H00000000"
           << ",0,0,0,0,100,100,0,0,1,0,0,7,0,0,0,1\n";
    }
    ss << "\n";

    // Events
    ss << "[Events]\n";
    ss << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";

    const double leftMargin = 50.0;
    const double cellWidth = 30;
    const double centerY = data.video.height / 2.0;
    const double midLine = (minLineIdx + maxLineIdx) / 2.0;

    // Build base text from lines
    std::string baseText;
    for (const std::string& line : lines) {
        if (!baseText.empty()) {
            baseText += "\\N";
        }
        baseText += line;
    }

    const int baseEndMs = data.audio.length * 1000;
    ss << "Dialogue: 1," << formatTime(0) << "," << formatTime(baseEndMs)
       << ",Base,,0,0,0,,{\\an4\\pos(" << leftMargin << "," << centerY << ")}"
       << baseText << "\n";

    // Create rhyme highlight dialogues
    for (const Token& token : data.tokens) {
        if (token.rhymeGroup.has_value()) {
            // Find the line this token belongs to
            const int linePos = lineIndexToPos[token.lineIndex];
            const std::string& lineText = lines[linePos];

            // Find the position of the token within the line
            size_t tokenCharPos = lineText.find(token.text);

            // Calculate X position: find location in string, multiply by cellWidth, add leftMargin
            const double tokenX = leftMargin + (tokenCharPos * cellWidth);
            const double boxWidth = token.text.length() * cellWidth;
            const double boxHeight = data.layout.fontSize + 12;

            // Calculate Y position based on line index
            const double lineY = centerY + (token.lineIndex - midLine) * data.layout.lineHeight;
            const double boxTop = lineY - boxHeight / 2.0;

            ss << "Dialogue: 0," << formatTime(token.startMs) << ","
               << formatTime(baseEndMs) << ",Rhyme_"
               << token.rhymeGroup.value() << ",,0,0,0,,{\\an7\\pos("
               << tokenX << "," << boxTop << ")\\p1}"
               << "m 0 0 l " << boxWidth << " 0 " << boxWidth << " " << boxHeight
               << " 0 " << boxHeight << "{\\p0}\n";
        }
    }

    // Set the string text
    text = ss.str();
}
