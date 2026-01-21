#ifndef INCLUDED_RSANS_ASS
#define INCLUDED_RSANS_ASS

#include <rsans_data.h>

#include <string>

struct Ass {
    std::string text;

    Ass() = delete;
    Ass(const ProjectData& data);
};

#endif
