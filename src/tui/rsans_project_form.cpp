#include <rsans_project_form.h>

#include <rsans_fonts.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

bool isHexColor(const std::string& value) {
    if (value.size() != 7 || value[0] != '#') {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(),
                       [](unsigned char c) { return std::isxdigit(c) != 0; });
}

// Parses a whole number, rejecting anything else (including "12abc")
std::string parseInteger(const std::string& text, int& out) {
    if (text.empty()) {
        out = 0;
        return "is required";
    }
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size()) {
            return "must be a whole number";
        }
        out = value;
        return "";
    } catch (const std::exception&) {
        return "must be a whole number";
    }
}

FormField textField(std::string section, std::string label, std::string key,
                    FieldKind kind) {
    FormField field;
    field.section = std::move(section);
    field.label = std::move(label);
    field.key = std::move(key);
    field.kind = kind;
    return field;
}

// A field that edits a std::string member. `member` is a generic lambda
// returning a reference to it, so it works on const and non-const projects.
template <typename Member>
FormField stringField(std::string section, std::string label, std::string key,
                      FieldKind kind, Member member) {
    FormField field = textField(std::move(section), std::move(label), std::move(key), kind);
    field.read = [member](const ProjectData& p) { return std::string(member(p)); };
    field.write = [member](ProjectData& p, const std::string& text) {
        member(p) = text;
        return std::string();
    };
    return field;
}

// A field that edits an int member, reached the same way
template <typename Member>
FormField integerField(std::string section, std::string label, std::string key, Member member) {
    FormField field = textField(std::move(section), std::move(label), std::move(key), FieldKind::Integer);
    field.read = [member](const ProjectData& p) {
        return std::to_string(member(p));
    };
    field.write = [member](ProjectData& p, const std::string& text) {
        return parseInteger(text, member(p));
    };
    return field;
}

FormField colorsField() {
    FormField field = textField("Colors", "Rhyme colors", "colorSwatch", FieldKind::Colors);
    field.read = [](const ProjectData& p) {
        std::string text;
        for (const Color& color : p.colorSwatch) {
            text += (text.empty() ? "" : ", ") + color.hex();
        }
        return text;
    };
    field.write = [](ProjectData& p, const std::string& text) {
        std::string normalized = text;
        std::replace(normalized.begin(), normalized.end(), ',', ' ');
        std::istringstream words(normalized);

        std::vector<Color> colors;
        std::string bad;
        std::string word;
        while (words >> word) {
            if (isHexColor(word)) {
                colors.emplace_back(word);
            } else if (bad.empty()) {
                bad = word;
            }
        }
        p.colorSwatch = std::move(colors);
        return bad.empty() ? std::string() : "not a #RRGGBB color: " + bad;
    };
    return field;
}

} // namespace

ProjectData defaultProject() {
    ProjectData project;
    project.header.titleSize = 64;
    project.header.artistSize = 36;
    project.video.width = 1080;
    project.video.height = 1920;
    project.video.background = "white";
    project.layout.fontSize = 48;
    project.layout.lineHeight = 60;
    project.colorSwatch = {Color("#FF8800"), Color("#00AAFF"), Color("#AA55FF"),
                           Color("#33CC66"), Color("#FF3366"), Color("#FFCC00")};

    // The build copies CMUdict to data/ in the source tree
    std::error_code ec;
    const std::filesystem::path bundledDict = std::filesystem::path("data") / "cmudict.dict";
    if (std::filesystem::is_regular_file(bundledDict, ec)) {
        project.cmudict = std::filesystem::absolute(bundledDict, ec).string();
    }
    return project;
}

ProjectForm::ProjectForm(ProjectData initial)
: d_project(std::move(initial)) {
    const auto add = [this](FormField field) { d_fields.push_back(std::move(field)); };

    add(stringField("Song", "Title", "header.title", FieldKind::Text,
                    [](auto& p) -> auto& { return p.header.title; }));
    add(integerField("Song", "Title size", "header.titleSize",
                     [](auto& p) -> auto& { return p.header.titleSize; }));
    add(stringField("Song", "Artist", "header.artist", FieldKind::Text,
                    [](auto& p) -> auto& { return p.header.artist; }));
    add(integerField("Song", "Artist size", "header.artistSize",
                     [](auto& p) -> auto& { return p.header.artistSize; }));
    add(stringField("Song", "Header font", "header.fontName", FieldKind::Text,
                    [](auto& p) -> auto& { return p.header.fontName; }));
    add(stringField("Song", "Header font file", "header.fontPath", FieldKind::Path,
                    [](auto& p) -> auto& { return p.header.fontPath; }));
    add(stringField("Song", "Cover image", "header.media", FieldKind::Path,
                    [](auto& p) -> auto& { return p.header.media; }));

    add(stringField("Audio", "Audio file", "audio.path", FieldKind::Path,
                    [](auto& p) -> auto& { return p.audio.path; }));

    add(integerField("Video", "Width", "video.width",
                     [](auto& p) -> auto& { return p.video.width; }));
    add(integerField("Video", "Height", "video.height",
                     [](auto& p) -> auto& { return p.video.height; }));
    add(stringField("Video", "Background", "video.background", FieldKind::Text,
                    [](auto& p) -> auto& { return p.video.background; }));

    add(stringField("Lyrics", "Lyrics font", "layout.fontName", FieldKind::Text,
                    [](auto& p) -> auto& { return p.layout.fontName; }));
    add(stringField("Lyrics", "Lyrics font file", "layout.fontPath", FieldKind::Path,
                    [](auto& p) -> auto& { return p.layout.fontPath; }));
    add(integerField("Lyrics", "Font size", "layout.fontSize",
                     [](auto& p) -> auto& { return p.layout.fontSize; }));
    add(integerField("Lyrics", "Line height", "layout.lineHeight",
                     [](auto& p) -> auto& { return p.layout.lineHeight; }));

    add(stringField("Analysis", "Whisper model", "model.path", FieldKind::Path,
                    [](auto& p) -> auto& { return p.model.base; }));
    add(stringField("Analysis", "Pronouncing dict", "cmudict", FieldKind::Path,
                    [](auto& p) -> auto& { return p.cmudict; }));

    add(colorsField());

    for (FormField& field : d_fields) {
        field.text = field.read(d_project);
    }
    apply();
}

void ProjectForm::measureAudioIfChanged() {
    if (d_project.audio.path == d_measuredAudioPath) {
        return;
    }
    d_measuredAudioPath = d_project.audio.path;
    d_project.audio.length = d_project.audio.path.empty()
        ? 0.0
        : getAudioDuration(d_project.audio.path);
}

void ProjectForm::apply() {
    d_parseErrors.clear();
    for (FormField& field : d_fields) {
        const std::string error = field.write(d_project, field.text);
        if (!error.empty()) {
            d_parseErrors[field.key] = error;
        }
    }
    measureAudioIfChanged();
    d_validation = d_project.validate();
    checkFontNames();
}

void ProjectForm::checkFontNames() {
    const std::pair<const char*, std::pair<const std::string*, const std::string*>> fonts[] = {
        {"header.fontName", {&d_project.header.fontName, &d_project.header.fontPath}},
        {"layout.fontName", {&d_project.layout.fontName, &d_project.layout.fontPath}},
    };
    for (const auto& [key, font] : fonts) {
        FontCheck& check = d_fontChecks[key];
        const bool known = !check.name.empty() || !check.path.empty();
        if (known && check.name == *font.first && check.path == *font.second) {
            continue;
        }
        check = {*font.first, *font.second, fontNameMismatch(*font.first, *font.second)};
    }
}

std::string ProjectForm::warningFor(const std::string& key) const {
    const auto check = d_fontChecks.find(key);
    return check == d_fontChecks.end() ? "" : check->second.warning;
}

std::vector<std::string> ProjectForm::warnings() const {
    std::vector<std::string> result;
    for (const auto& [key, check] : d_fontChecks) {
        if (!check.warning.empty()) {
            result.push_back(std::string(key == "header.fontName" ? "the header" : "the lyrics") +
                             " font's name doesn't match its file");
        }
    }
    return result;
}

void ProjectForm::reset(ProjectData project) {
    d_project = std::move(project);
    // Its length was measured or read when it was loaded
    d_measuredAudioPath = d_project.audio.path;
    for (FormField& field : d_fields) {
        field.text = field.read(d_project);
    }
    apply();
}

bool ProjectForm::setFieldText(const std::string& key, const std::string& text) {
    for (FormField& field : d_fields) {
        if (field.key == key) {
            field.text = text;
            apply();
            return true;
        }
    }
    return false;
}

bool ProjectForm::setTokenText(std::size_t index, const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t");
    if (index >= d_project.tokens.size() || first == std::string::npos) {
        return false;
    }
    const std::size_t last = text.find_last_not_of(" \t");
    d_project.tokens[index].text = text.substr(first, last - first + 1);
    d_validation = d_project.validate();
    return true;
}

bool ProjectForm::setTokenRhyme(std::size_t index, std::optional<int> rhymeIndex) {
    if (index >= d_project.tokens.size() || (rhymeIndex && *rhymeIndex < 0)) {
        return false;
    }
    d_project.tokens[index].rhymeIndex = rhymeIndex;
    d_validation = d_project.validate();
    return true;
}

bool ProjectForm::setTokenTiming(std::size_t index, int startMs, int endMs) {
    if (index >= d_project.tokens.size() || startMs < 0 || endMs < startMs) {
        return false;
    }
    d_project.tokens[index].startMs = startMs;
    d_project.tokens[index].endMs = endMs;
    d_validation = d_project.validate();
    return true;
}

bool ProjectForm::removeToken(std::size_t index) {
    if (index >= d_project.tokens.size()) {
        return false;
    }
    d_project.tokens.erase(d_project.tokens.begin() + static_cast<std::ptrdiff_t>(index));
    d_validation = d_project.validate();
    return true;
}

bool ProjectForm::insertToken(std::size_t index, const std::string& text, int lineIndex,
                              std::optional<int> rhymeIndex) {
    std::vector<Token>& tokens = d_project.tokens;
    const std::size_t first = text.find_first_not_of(" \t");
    if (index > tokens.size() || first == std::string::npos || (rhymeIndex && *rhymeIndex < 0)) {
        return false;
    }
    const std::size_t last = text.find_last_not_of(" \t");

    Token* before = index > 0 ? &tokens[index - 1] : nullptr;
    Token* after = index < tokens.size() ? &tokens[index] : nullptr;
    // Placed next to the word on its own line: the one before, unless the
    // new word starts a line that the word after it is on
    const bool besideBefore = before != nullptr &&
        (before->lineIndex == lineIndex || after == nullptr || after->lineIndex != lineIndex);

    constexpr int longestGapMs = 1000;
    int startMs = 0;
    int endMs = 0;
    const int gapMs = before != nullptr && after != nullptr ? after->startMs - before->endMs : 0;
    if (gapMs > 0) {
        const int length = std::min(gapMs, longestGapMs);
        startMs = besideBefore ? before->endMs : after->startMs - length;
        endMs = startMs + length;
    } else if (besideBefore) {
        // Shares the word before: its second half
        const int middle = before->startMs + (before->endMs - before->startMs) / 2;
        startMs = middle;
        endMs = before->endMs;
        before->endMs = middle;
    } else if (after != nullptr) {
        // Shares the word after: its first half
        const int middle = after->startMs + (after->endMs - after->startMs) / 2;
        startMs = after->startMs;
        endMs = middle;
        after->startMs = middle;
    }

    // A fresh id, so ids stay unique
    int id = 0;
    for (const Token& token : tokens) {
        id = std::max(id, token.id + 1);
    }
    tokens.insert(tokens.begin() + static_cast<std::ptrdiff_t>(index),
                  Token(id, text.substr(first, last - first + 1), startMs, endMs, lineIndex, rhymeIndex));
    d_validation = d_project.validate();
    return true;
}

void ProjectForm::setTokens(std::vector<Token> tokens) {
    d_project.tokens = std::move(tokens);
    d_validation = d_project.validate();
}

std::vector<std::string> ProjectForm::errorsFor(const std::string& key) const {
    const auto parseError = d_parseErrors.find(key);
    if (parseError != d_parseErrors.end()) {
        return {parseError->second};
    }

    std::vector<std::string> errors;
    for (const ValidationError& error : d_validation) {
        if (error.field == key) {
            errors.push_back(error.message);
        }
    }
    return errors;
}
