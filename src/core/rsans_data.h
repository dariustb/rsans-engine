#ifndef INCLUDED_RSANS_DATA
#define INCLUDED_RSANS_DATA

#include <optional>
#include <string>
#include <vector>

struct Token {
    int id;
    std::string text;
    int startMs;
    int endMs;
    int lineIndex;
    std::optional<int> rhymeIndex;

    Token() = delete;
    Token(
        int id,
        const std::string& text,
        int startMs,
        int endMs,
        int lineIndex,
        const std::optional<int>& rhymeIndex
    );
};

class Color {
  private:
    std::string d_hex;
    std::string d_ass;

    std::string toAss(const std::string& hexValue);
  public:
    std::string hex() const;
    std::string ass() const;

    Color() = delete;
    Color(const std::string& hexValue);
};

struct ValidationError {
    std::string field;    // Dotted path matching the JSON keys, e.g. "audio.path"
    std::string message;
};

struct ProjectData {
    struct HeaderData {
        std::string fontName;
        std::string fontPath;
        std::string title;
        int titleSize = 0;
        std::string artist;
        int artistSize = 0;
        std::string media;
    } header;

    struct AudioData {
        std::string path;
        double length = 0.0;
    } audio;

    struct VideoConfig {
        int width = 0;
        int height = 0;
        std::string background;
    } video;
    
    struct LayoutConfig {
        std::string fontName;
        std::string fontPath;
        int fontSize = 0;
        int lineHeight = 0;
    } layout;

    struct ModelPaths {
        // Any Whisper model file (tiny to large); "base" is only the field's name
        std::string base;
    } model;

    std::string cmudict;

    std::vector<Token> tokens;

    std::vector<Color> colorSwatch;

    ProjectData() = default;  // Blank project: not valid until mandatory values are filled in.
    ProjectData(const std::string& jsonContent);

    // Copyable and movable; every member copies or moves on its own
    ProjectData(const ProjectData& other) = default;
    ProjectData(ProjectData&& other) noexcept = default;
    ProjectData& operator=(const ProjectData& other) = default;
    ProjectData& operator=(ProjectData&& other) noexcept = default;
    ~ProjectData() = default;

    std::string toJson() const;

    // Checks that mandatory values are present and that every value present is
    // usable. Returns one error per problem, or an empty vector if valid.
    std::vector<ValidationError> validate() const;
    bool isValid() const;

    // Per-step checks: only what each step reads, so a project can be ready
    // for one step but not another (e.g. analyze before any fonts are chosen).
    // Each returns one error per problem, or an empty vector if ready.
    std::vector<ValidationError> validateForAnalyze() const;  // audio.path, model.path
    std::vector<ValidationError> validateForRhyme() const;    // cmudict, tokens
    std::vector<ValidationError> validateForExport() const;   // all visual settings, audio, tokens
    std::vector<ValidationError> validateForFull() const;     // all of the above, except tokens
};

// Returns the length of the audio file in seconds, or -1.0 if libsndfile
// cannot open it.
double getAudioDuration(const std::string& audioPath);

#endif
