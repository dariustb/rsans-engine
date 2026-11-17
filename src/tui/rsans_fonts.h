#ifndef INCLUDED_RSANS_FONTS
#define INCLUDED_RSANS_FONTS

#include <optional>
#include <string>
#include <vector>

// A font the lyrics can be rendered with. `family` is what the subtitles
// name (libass looks it up by family), `path` is the file it comes from.
struct FontInfo {
    std::string family;
    std::string style;
    std::string path;
};

// Lists the fonts installed on the system (via fontconfig), one entry per
// family, using that family's regular (upright, normal weight) face. Only
// TrueType and OpenType/CFF fonts are included, sorted by family name,
// ignoring case. Takes a moment the first time (fontconfig reads its cache).
std::vector<FontInfo> scanSystemFonts();

// Reads the family and style from a font file, which doesn't have to be
// installed. Returns nothing if it isn't a usable font file.
std::optional<FontInfo> fontFromFile(const std::string& path);

// The fonts whose family contains every word of `query`, ignoring case.
// Families starting with the query come first; otherwise the order is kept.
// An empty query matches everything. Returns indexes into `fonts`.
std::vector<std::size_t> filterFonts(const std::vector<FontInfo>& fonts, const std::string& query);

#endif
