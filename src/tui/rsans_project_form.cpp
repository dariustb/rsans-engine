#include <rsans_project_form.h>

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
    project.video.width = 1920;
    project.video.height = 1080;
    project.video.background = "black";
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
