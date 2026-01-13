#include <rsans_ass.h>

#include <rsans_data.h>

#include <algorithm>
#include <sstream>


std::map<int, std::vector<Token>> groupTokensByLine(
    const std::vector<Token>& tokens) {
    std::map<int, std::vector<Token>> lineTokens;
    for (const auto& token : tokens) {
        lineTokens[token.lineIndex].push_back(token);
    }
    return lineTokens;
}

// TODO: Pass refs from ProjectData
// TODO: Can this be a constructor for LineData???
std::map<int, LineData> processLines(
    const std::map<int, std::vector<Token>>& lineTokens,
    const ProjectData::VideoConfig& video, const ProjectData::LayoutConfig& layout, int& maxEndMs) {

    const int playResX = video.width;
    const int playResY = video.height;
    const double centerY = playResY / 2.0;
    const double cellWidth = 30;

    const int minLine = lineTokens.begin()->first;
    const int maxLine = lineTokens.rbegin()->first;
    double midLine = (minLine + maxLine) / 2.0;

    std::map<int, LineData> linesData;
    maxEndMs = 0;

    for (auto& [lineIndex, tokens] : lineTokens) {
        // TODO: Get rid of the sorting of the Tokens.
        // Does nothing at best, mixes up the words at worst
        std::vector<Token> sortedTokens = tokens;
        std::sort(sortedTokens.begin(), sortedTokens.end(),
                  [](const Token& a, const Token& b) {
                      return a.startMs < b.startMs;
                  });

        LineData lineData;
        int currentOffset = 0;

        for (size_t i = 0; i < sortedTokens.size(); ++i) {
            TokenWithOffset tko;
            tko.token = sortedTokens[i];
            tko.columnOffset = currentOffset;

            lineData.tokens.push_back(tko);
            lineData.lineText += sortedTokens[i].text;
            currentOffset += sortedTokens[i].text.length();

            if (sortedTokens[i].endMs > maxEndMs) {
                maxEndMs = sortedTokens[i].endMs;
            }

            if (i < sortedTokens.size() - 1) {
                lineData.lineText += " ";
                currentOffset += 1;
            }
        }

        lineData.lineY = centerY + (lineIndex - midLine) * layout.lineHeight;
        double lineWidth = lineData.lineText.length() * cellWidth;
        lineData.lineStartX = (playResX - lineWidth) / 2.0; // TODO: not needed b/c of leftMargin

        for (auto& tko : lineData.tokens) {
            tko.x = lineData.lineStartX + tko.columnOffset * cellWidth;
            tko.y = lineData.lineY;
        }

        linesData[lineIndex] = lineData;
    }

    return linesData;
}

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

std::string buildScriptInfo(int playResX, int playResY) {
    std::ostringstream ss;
    ss << "[Script Info]\n";
    ss << "Title: Generated ASS\n";
    ss << "ScriptType: v4.00+\n";
    ss << "PlayResX: " << playResX << "\n";
    ss << "PlayResY: " << playResY << "\n";
    ss << "\n";
    return ss.str();
}

// TODO: Make a var to name each value passed here instead of literals
std::string buildStyles(const std::string& fontName, int fontSize,
                        const std::map<std::string, ProjectData::RhymeStyle>& rhymeStyles) {
    std::ostringstream ss;
    ss << "[V4+ Styles]\n";
    ss << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
       << "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
       << "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
       << "Alignment, MarginL, MarginR, MarginV, Encoding\n";

    ss << "Style: Base," << fontName << "," << fontSize
       << ",&H00606060,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,0,0,4,0,0,0,1\n";

    for (const auto& [group, style] : rhymeStyles) {
        std::string assColor = hexToAssColor(style.color);
        ss << "Style: Rhyme_" << group << "," << fontName << "," << fontSize
           << "," << assColor << ",&H000000FF,&H00000000,&H00000000"
           << ",0,0,0,0,100,100,0,0,1,0,0,7,0,0,0,1\n";
    }
    ss << "\n";
    return ss.str();
}

std::string buildEvents(const std::map<int, LineData>& linesData, double centerX,
                        double centerY, int maxEndMs, int fontSize) {
    std::ostringstream ss;
    ss << "[Events]\n";
    ss << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";

    std::string baseText;
    const double leftMargin = 50.0;
    const double cellWidth = 30;

    for (const auto& [lineIndex, lineData] : linesData) {
        if (!baseText.empty()) {
            baseText += "\\N";
        }
        baseText += lineData.lineText;
    }

    const int baseEndMs = maxEndMs + 1000;
    ss << "Dialogue: 1," << formatTime(0) << "," << formatTime(baseEndMs)
       << ",Base,,0,0,0,,{\\an4\\pos(" << leftMargin << "," << centerY << ")}"
       << baseText << "\n";

    for (const auto& [lineIndex, lineData] : linesData) {
        for (const auto& tko : lineData.tokens) {
            if (tko.token.rhymeGroup.has_value()) {
                const double tokenX = leftMargin + (tko.columnOffset * cellWidth);
                const double boxWidth = tko.token.text.length() * cellWidth;
                const double boxHeight = fontSize + 12; // fontSize + padding
                const double boxTop = centerY - boxHeight / 2.0;

                ss << "Dialogue: 0," << formatTime(tko.token.startMs) << ","
                   << formatTime(baseEndMs) << ",Rhyme_"
                   << tko.token.rhymeGroup.value() << ",,0,0,0,,{\\an7\\pos("
                   << tokenX << "," << boxTop << ")\\p1}"
                   << "m 0 0 l " << boxWidth << " 0 " << boxWidth << " " << boxHeight
                   << " 0 " << boxHeight << "{\\p0}\n";
            }
        }
    }

    return ss.str();
}

// TODO: Could this be a constructor to an ASS struct?
std::string generateAss(const ProjectData& data) {
    std::map<int, std::vector<Token>> lineTokens = groupTokensByLine(data.tokens);

    // TODO: maxEndMs should be replaced with a songLength that's explicit in the JSON
    int maxEndMs = 0;
    std::map<int, LineData> linesData =
        processLines(lineTokens, data.video, data.layout, maxEndMs);

    const double centerX = data.video.width / 2.0;
    const double centerY = data.video.height / 2.0;

    const std::string scriptInfo = buildScriptInfo(data.video.width, data.video.height);
    const std::string styles = buildStyles(data.layout.fontName, data.layout.fontSize,
                                     data.rhymeStyles);
    const std::string events = buildEvents(linesData, centerX, centerY, maxEndMs,
                                     data.layout.fontSize);

    return scriptInfo + styles + events;
}
