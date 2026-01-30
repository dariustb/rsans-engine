#ifndef INCLUDED_RSANS_FONT
#define INCLUDED_RSANS_FONT

#include <ft2build.h>
#include FT_FREETYPE_H

#include <string>

class FTFont {
  private:
    FT_Library d_library;
    FT_Face    d_face;
  public:
    int getFontPixelHeight() const;

    FTFont() = delete;
    FTFont(const FTFont&) = delete;
    FTFont& operator=(const FTFont&) = delete;
    FTFont(FTFont&& other) = delete;
    FTFont& operator=(FTFont&& other) = delete;

    FTFont(const std::string& fontPath, int fontSize);
    ~FTFont();
};

#endif
