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

#endif
