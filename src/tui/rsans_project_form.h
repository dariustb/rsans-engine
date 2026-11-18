#ifndef INCLUDED_RSANS_PROJECT_FORM
#define INCLUDED_RSANS_PROJECT_FORM

#include <rsans_data.h>

#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

enum class FieldKind {
    Text,     // Free text
    Path,     // A file path
    Integer,  // A whole number
    Colors    // A list of #RRGGBB colors, separated by commas or spaces
};

// One editable setting. The UI edits `text` directly; ProjectForm::apply()
// parses it into the project.
struct FormField {
    std::string section;  // Heading the field is grouped under, e.g. "Song"
    std::string label;    // Shown next to the input, e.g. "Title"
    std::string key;      // Validation field name, e.g. "header.title"
    FieldKind kind;
    std::string text;

    // How the field reads from and writes to the project. write() returns a
    // parse error message, or "" if the text was valid.
    std::function<std::string(const ProjectData&)> read;
    std::function<std::string(ProjectData&, const std::string&)> write;
};

// The project being edited in the TUI, plus the text of every field and the
// errors to show next to each one. Independent of FTXUI so it can be tested.
class ProjectForm {
  private:
    ProjectData d_project;
    // A deque so each field's text keeps its address (FTXUI inputs point at it)
    std::deque<FormField> d_fields;
    std::map<std::string, std::string> d_parseErrors;
    std::vector<ValidationError> d_validation;
    std::string d_measuredAudioPath;

    void measureAudioIfChanged();

  public:
    explicit ProjectForm(ProjectData initial);

    ProjectForm(const ProjectForm&) = delete;
    ProjectForm& operator=(const ProjectForm&) = delete;
    ProjectForm(ProjectForm&&) = delete;
    ProjectForm& operator=(ProjectForm&&) = delete;
    ~ProjectForm() = default;

    std::deque<FormField>& fields() { return d_fields; }
    const ProjectData& project() const { return d_project; }

    // Parses every field's text into the project, measures the audio's
    // length when its path changes, and re-runs validation
    void apply();

    // Replaces the whole project (new or loaded) and refills every field
    void reset(ProjectData project);

    // Sets one field's text (by validation key) and applies the form.
    // Returns false if there is no such field.
    bool setFieldText(const std::string& key, const std::string& text);

    // Replaces the tokens, e.g. with the results of analyze or rhyme
    void setTokens(std::vector<Token> tokens);

    // Corrects one transcribed word (spaces at either end are removed).
    // False if there's no such word or the text is empty.
    bool setTokenText(std::size_t index, const std::string& text);
    // Removes one word, e.g. one that was never sung. False if there's no such word.
    bool removeToken(std::size_t index);

    // Errors to show for one field: its parse error if the text couldn't be
    // read, otherwise any validation errors for it
    std::vector<std::string> errorsFor(const std::string& key) const;

    // Every validation error from the last apply()
    const std::vector<ValidationError>& validation() const { return d_validation; }
};

// A new project with sensible starting values for the settings that have
// them (video size, text sizes, colors, and the bundled dictionary if found)
ProjectData defaultProject();

#endif
