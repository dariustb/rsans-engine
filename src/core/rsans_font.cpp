#include <rsans_font.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdexcept>
#include <utility>

FTFont::FTFont(const std::string& fontPath, int fontSize)
    : d_library(nullptr)
    , d_face(nullptr)
{
    if (FT_Init_FreeType(&d_library)) {
        throw std::runtime_error("Failed to initialize FreeType library");
    }

    if (FT_New_Face(d_library, fontPath.c_str(), 0, &d_face)) {
        FT_Done_FreeType(d_library);
        throw std::runtime_error("Failed to load font: " + fontPath);
    }

    if (FT_Set_Pixel_Sizes(d_face, 0, fontSize)) {
        FT_Done_Face(d_face);
        FT_Done_FreeType(d_library);
        throw std::runtime_error("Failed to set font size");
    }
}

FTFont::~FTFont() {
    if (d_face) {
        FT_Done_Face(d_face);
    }
    if (d_library) {
        FT_Done_FreeType(d_library);
    }
}

FTFont::FTFont(FTFont&& other) noexcept
    : d_library(std::exchange(other.d_library, nullptr))
    , d_face(std::exchange(other.d_face, nullptr))
{
}

FTFont& FTFont::operator=(FTFont&& other) noexcept {
    if (this != &other) {
        if (d_face) {
            FT_Done_Face(d_face);
        }
        if (d_library) {
            FT_Done_FreeType(d_library);
        }
        d_library = std::exchange(other.d_library, nullptr);
        d_face = std::exchange(other.d_face, nullptr);
    }
    return *this;
}

int FTFont::getFontPixelHeight() const {
    // FreeType metrics are in 26.6 fixed-point format, so shift right by 6
    return (d_face->size->metrics.ascender - d_face->size->metrics.descender) >> 6;
}

int FTFont::getStringPixelWidth(const std::string& text) {
    int totalWidth = 0;

    for (char c : text) {
        if (FT_Load_Char(d_face, c, FT_LOAD_DEFAULT)) {
            continue;  // Skip characters that fail to load
        }
        // advance.x is in 26.6 fixed-point format
        totalWidth += d_face->glyph->advance.x >> 6;
    }

    return totalWidth;
}
