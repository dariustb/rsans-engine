#ifndef INCLUDED_RSANS_TITLE
#define INCLUDED_RSANS_TITLE

#include <ftxui/dom/elements.hpp>

#include <string>

// The RSANS title in colored block letters (from title_text.py)
ftxui::Element titleArt();
constexpr int titleArtWidth = 94;
constexpr int titleArtHeight = 12;

// One line of text with ANSI color codes ("\033[0;94;44m...") as an FTXUI
// element. Handles reset (0), the 16 foreground colors (30-37, 90-97), the
// 16 background colors (40-47, 100-107), and default colors (39, 49); other
// codes are ignored.
ftxui::Element ansiLine(const std::string& line);

#endif
