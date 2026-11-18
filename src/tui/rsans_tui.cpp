#include <rsans_tui.h>

#include <rsans_export.h>
#include <rsans_io.h>
#include <rsans_process.h>
#include <rsans_rhyme.h>
#include <rsans_title.h>
#include <rsans_whisper.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/dom/requirement.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <cstdlib>
#include <system_error>
#include <fstream>
#include <cctype>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ftxui;

namespace {

constexpr int labelWidth = 20;

// The validation message for an empty mandatory field (see rsans_data.cpp)
const std::string requiredMessage = "is required";

std::string percent(double fraction) {
    return std::to_string(static_cast<int>(fraction * 100.0 + 0.5)) + "%";
}

std::string formatTime(int ms) {
    const int seconds = ms / 1000;
    std::array<char, 16> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%d:%02d", seconds / 60, seconds % 60);
    return buffer.data();
}

// "#RRGGBB" to an FTXUI color (the swatch is validated, but stay safe)
ftxui::Color toColor(const std::string& hex) {
    if (hex.size() != 7) {
        return ftxui::Color::Default;
    }
    try {
        const auto channel = [&](std::size_t pos) {
            return static_cast<uint8_t>(std::stoi(hex.substr(pos, 2), nullptr, 16));
        };
        return ftxui::Color::RGB(channel(1), channel(3), channel(5));
    } catch (const std::exception&) {
        return ftxui::Color::Default;
    }
}

std::string stateText(const StepRunner& runner) {
    switch (runner.state()) {
    case StepRunner::State::Idle:      return "Idle";
    case StepRunner::State::Running:   return runner.cancelRequested() ? "Cancelling" : "Running";
    case StepRunner::State::Succeeded: return "Done";
    case StepRunner::State::Failed:    return "Failed";
    case StepRunner::State::Cancelled: return "Cancelled";
    }
    return "";
}

// Forwards whisper.cpp's log to the step's log, without debug noise
WhisperLogHandler whisperLog(const StepContext& context) {
    return [onLog = context.onLog](WhisperLogLevel level, const std::string& line) {
        if (level != WhisperLogLevel::Debug) {
            onLog(line);
        }
    };
}

// Scales a sub-step's 0..1 progress into [from, to] of the whole run
ProgressHandler scaledProgress(const ProgressHandler& onProgress, double from, double to) {
    return [onProgress, from, to](double fraction) {
        onProgress(from + (to - from) * fraction);
    };
}

std::vector<std::string> describe(const std::vector<ValidationError>& errors) {
    std::vector<std::string> lines;
    lines.reserve(errors.size());
    for (const ValidationError& error : errors) {
        lines.push_back(error.field + ": " + error.message);
    }
    return lines;
}

// The form keys a font setting is stored under
struct FontKeys {
    const char* name;
    const char* path;
};
FontKeys fontKeys(RsansApp::FontTarget target) {
    return target == RsansApp::FontTarget::Header
        ? FontKeys{"header.fontName", "header.fontPath"}
        : FontKeys{"layout.fontName", "layout.fontPath"};
}

// File types the dialog lists for each path setting (empty: every file)
const std::vector<std::string> imageTypes = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tga"};
const std::vector<std::string> audioTypes = {".wav", ".flac", ".ogg", ".oga", ".opus", ".mp3", ".aif", ".aiff"};
const std::vector<std::string> modelTypes = {".bin"};
const std::vector<std::string> dictTypes = {".dict", ".txt"};
const std::vector<std::string> fontTypes = {".ttf", ".otf", ".ttc"};
const std::vector<std::string> videoTypes = {".mp4", ".mkv", ".mov", ".webm"};

// Colors a dialog's frame so it stands out from the screen behind it: the
// box-drawing characters around its edge, and the separator lines across it.
// The title and the contents keep their own colors.
class FrameColor : public Node {
  public:
    FrameColor(Element child, ftxui::Color frameColor)
    : Node(Elements{std::move(child)})
    , d_color(frameColor) {}

    void ComputeRequirement() override {
        Node::ComputeRequirement();
        requirement_ = children_[0]->requirement();
    }

    void SetBox(Box box) override {
        Node::SetBox(box);
        children_[0]->SetBox(box);
    }

    void Render(Screen& screen) override {
        Node::Render(screen);
        for (int y = box_.y_min; y <= box_.y_max; ++y) {
            // Edges, plus whole rows that are separators. (FTXUI joins │ and ─
            // into ├ only after everything is drawn, so look for the ─.)
            const bool edgeRow = y == box_.y_min || y == box_.y_max;
            const bool separatorRow = box_.x_min + 1 <= box_.x_max &&
                                      screen.PixelAt(box_.x_min + 1, y).character == "\u2500";
            for (int x = box_.x_min; x <= box_.x_max; ++x) {
                const bool edge = edgeRow || separatorRow || x == box_.x_min || x == box_.x_max;
                Pixel& pixel = screen.PixelAt(x, y);
                if (edge && isBoxDrawing(pixel.character)) {
                    pixel.foreground_color = d_color;
                }
            }
        }
    }

  private:
    ftxui::Color d_color;

    // U+2500-U+257F, which is E2 94 80 to E2 95 BF in UTF-8
    static bool isBoxDrawing(const std::string& character) {
        return character.size() == 3 && static_cast<unsigned char>(character[0]) == 0xE2 &&
               (static_cast<unsigned char>(character[1]) == 0x94 ||
                static_cast<unsigned char>(character[1]) == 0x95);
    }
};

// Space between the editor's border and its contents, on the left and right
constexpr int editorPadding = 2;
Element paddedX(Element content) {
    return hbox({
        text(std::string(editorPadding, ' ')),
        std::move(content) | flex,
        text(std::string(editorPadding, ' ')),
    });
}

// A pop-up dialog: a titled window with a yellow frame
Element dialogWindow(const std::string& title, Element content) {
    return std::make_shared<FrameColor>(window(text(title), std::move(content)), ftxui::Color::YellowLight);
}

// Project files are JSON, saved with their own extension. Older .json
// projects can still be opened.
const std::string projectExtension = ".rsans";

// The path with the project extension: ".json" is replaced, and any other
// extension is kept with ".rsans" added ("my.song" -> "my.song.rsans")
std::string withProjectExtension(const std::string& path) {
    std::filesystem::path result(path);
    if (result.extension() == projectExtension) {
        return result.string();
    }
    if (result.extension() == ".json") {
        result.replace_extension(projectExtension);
        return result.string();
    }
    return result.string() + projectExtension;
}

// A file name from free text: "My Song!" -> "my-song" ("" if nothing's left)
std::string fileNameFrom(const std::string& text) {
    std::string name;
    for (const char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
            name += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (!name.empty() && name.back() != '-') {
            name += '-';
        }
    }
    while (!name.empty() && name.back() == '-') {
        name.pop_back();
    }
    return name;
}

// Shortens a path to at most `width` characters by cutting from the left, so
// the end (the part that differs) stays visible
std::string elideLeft(const std::string& path, std::size_t width) {
    if (path.size() <= width || width < 4) {
        return path;
    }
    return "..." + path.substr(path.size() - (width - 3));
}

InputOption singleLineInput() {
    InputOption option = InputOption::Default();
    option.multiline = false;
    return option;
}

// FTXUI's input uses Up/Down to move its cursor within the text, so leaving a
// field with a value takes two presses. This passes Up/Down straight to the
// surrounding container instead, so one press always moves one row.
class VerticalArrowsMoveFocus : public ComponentBase {
  public:
    explicit VerticalArrowsMoveFocus(Component input) { Add(std::move(input)); }

    bool OnEvent(Event event) override {
        if (event == Event::ArrowUp || event == Event::ArrowDown) {
            return false;
        }
        return ComponentBase::OnEvent(event);
    }
};

Component TextInput(StringRef content, InputOption option) {
    return Make<VerticalArrowsMoveFocus>(Input(std::move(content), std::move(option)));
}

Component TextInput(StringRef content, StringRef placeholder, InputOption option) {
    option.placeholder = std::move(placeholder);
    return TextInput(std::move(content), std::move(option));
}

} // namespace

RsansApp::RsansApp(std::string ffmpeg, StepRunner::Post post, std::function<void()> quit)
: whisperDir(defaultWhisperDir())
, ffmpegPath(std::move(ffmpeg))
, d_form(defaultProject())
, d_quit(std::move(quit))
, d_post(post)
, d_startEntries({"New project", "Load project", "Credits", "Quit"})
, d_tabNames({"Project", "Run", "Lyrics"})
, d_runner(std::move(post)) {
    refreshWhisperModels();

    Component tabs = Container::Tab({makeProjectTab(), makeRunTab(), makeLyricsTab()}, &d_tab);
    Component tabBar = Toggle(&d_tabNames, &d_tab);
    Component saveButton = Button("Save", [this] { save(); }, ButtonOption::Ascii());
    Component saveAsButton = Button("Save as", [this] { saveAs(); }, ButtonOption::Ascii());
    Component menuButton = Button("Menu", [this] { goToMenu(); }, ButtonOption::Ascii());
    Component header = Container::Horizontal({tabBar, saveButton, saveAsButton, menuButton});
    Component layout = Container::Vertical({header, tabs});

    Component rendered = Renderer(layout, [this, tabBar, saveButton, saveAsButton, menuButton, tabs] {
        // The project file's name, "*" while there are unsaved changes
        const bool unsaved = hasUnsavedChanges();
        if (unsaved) {
            d_saveMessage.clear();
        }
        const std::string name = d_projectPath.empty()
            ? "New project"
            : std::filesystem::path(d_projectPath).filename().string();
        Element fileStatus = d_saveMessage.empty()
            ? text(name + (unsaved ? " *" : "")) | (unsaved ? color(ftxui::Color::YellowLight) : dim)
            : text(d_saveMessage) | color(ftxui::Color::Green);
        return vbox({
            hbox({text(" RSANS ") | bold | inverted, text(" "), tabBar->Render(), filler(),
                  fileStatus, text("  "), saveButton->Render(), saveAsButton->Render(),
                  // Not while a step runs: its results belong to this project
                  d_runner.isRunning() ? menuButton->Render() | dim : menuButton->Render(), text("  "),
                  text(stateText(d_runner) + " ") | dim}),
            separator(),
            paddedX(tabs->Render()) | flex,
            separator(),
            paddedX(text("Arrows/Tab: move   Enter: press   Ctrl+S: save   Esc: menu   Ctrl+C: quit") | dim),
        }) | border;
    });

    // The font picker opens on top of everything else, and the file dialog on
    // top of that (the picker can open it to pick a font file)
    Component screens = Container::Tab({makeStartScreen(), makeCredits(), rendered}, &d_screen);
    Component withPicker = Modal(screens, makeFontPicker(), &d_pickerOpen);
    Component withModels = Modal(withPicker, makeModelPicker(), &d_modelPickerOpen);
    Component withWords = Modal(withModels, makeWordEditor(), &d_wordEditorOpen);
    Component withPrompt = Modal(withWords, makeQuitPrompt(), &d_quitPromptOpen);
    Component withDialogs = Modal(withPrompt, makeFileDialog(), &d_fileDialogOpen);

    d_root = CatchEvent(withDialogs, [this](const Event& event) {
        if (event == Event::CtrlC) {
            // Unsaved work: ask first (Ctrl+C at the prompt quits anyway)
            if (hasUnsavedChanges() && !d_quitPromptOpen) {
                askBeforeLeaving(Leave::Quit);
                return true;
            }
            quitNow();
            return true;
        }
        const bool dialogOpen = d_pickerOpen || d_modelPickerOpen || d_fileDialogOpen || d_quitPromptOpen ||
                                d_wordEditorOpen;
        const bool inEditor = d_screen == static_cast<int>(Screen::Editor);
        if (event == Event::CtrlS && inEditor && !dialogOpen) {
            save();
            return true;
        }
        // Esc closes an open dialog (handled by the dialog); otherwise it's the menu
        if (event == Event::Escape && inEditor && !dialogOpen) {
            goToMenu();
            return true;
        }
        return false;
    });
    markSaved();  // A new, untouched project has nothing to save
}

void RsansApp::quitNow() {
    d_runner.cancel();  // The runner's destructor waits for it to stop
    d_quit();
}

void RsansApp::showStartScreen() {
    d_wordEditorOpen = false;
    d_pickerOpen = false;
    d_modelPickerOpen = false;
    d_fileDialogOpen = false;
    d_quitPromptOpen = false;
    d_startSelected = 0;
    d_screen = static_cast<int>(Screen::Start);
}

void RsansApp::leave(Leave how) {
    if (how == Leave::Quit) {
        quitNow();
    } else if (how == Leave::Menu) {
        showStartScreen();
    }
}

void RsansApp::askBeforeLeaving(Leave how) {
    d_wordEditorOpen = false;
    d_pickerOpen = false;
    d_modelPickerOpen = false;
    d_fileDialogOpen = false;
    d_promptLeave = how;
    d_quitPromptOpen = true;
}

bool RsansApp::goToMenu() {
    if (d_screen != static_cast<int>(Screen::Editor) || d_runner.isRunning()) {
        return false;
    }
    if (hasUnsavedChanges()) {
        askBeforeLeaving(Leave::Menu);
    } else {
        showStartScreen();
    }
    return true;
}

void RsansApp::markSaved() {
    d_savedJson = d_form.project().toJson();
}

bool RsansApp::hasUnsavedChanges() const {
    return d_screen == static_cast<int>(Screen::Editor) && d_form.project().toJson() != d_savedJson;
}

std::string RsansApp::saveProject(const std::string& path) {
    // Write a temporary file and rename it over the project, so a failed write
    // (e.g. a full disk) never leaves a half-written project behind
    const std::filesystem::path target(path);
    const std::filesystem::path temporary = target.string() + ".tmp";
    const std::string json = d_form.project().toJson();
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << json;
        out.close();
        if (!out) {
            std::error_code ec;
            std::filesystem::remove(temporary, ec);
            return "Can't save to " + path;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temporary, target, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        return "Can't save to " + path + ": " + ec.message();
    }

    d_projectPath = path;
    d_savedJson = json;
    d_saveMessage = "Saved " + target.filename().string();
    d_overwriteCandidate.clear();
    const Leave then = d_afterSave;
    d_afterSave = Leave::None;
    leave(then);
    return "";
}

void RsansApp::save() {
    d_afterSave = Leave::None;  // Only saving from the unsaved-changes prompt leaves
    saveOrAsk();
}

void RsansApp::saveAs() {
    d_afterSave = Leave::None;
    askWhereToSave();
}

void RsansApp::saveOrAsk() {
    if (d_projectPath.empty()) {
        askWhereToSave();
        return;
    }
    const std::string error = saveProject(d_projectPath);
    if (!error.empty()) {
        d_saveMessage.clear();
        askWhereToSave();  // Somewhere else, then
        d_fileError = error;
    }
}

void RsansApp::askWhereToSave() {
    // Suggest a name from the song title
    std::string current = d_projectPath;
    if (current.empty()) {
        const std::string name = fileNameFrom(d_form.project().header.title);
        current = (std::filesystem::current_path() / ((name.empty() ? "project" : name) + projectExtension)).string();
    }
    d_overwriteCandidate.clear();
    openFileDialog("Save project as", {projectExtension}, true, current, [this](const std::string& chosen) {
        const std::filesystem::path path(withProjectExtension(chosen));
        // Replacing another file needs a second Enter
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && path.string() != d_projectPath &&
            path.string() != d_overwriteCandidate) {
            d_overwriteCandidate = path.string();
            return path.filename().string() + " already exists. Press Enter again to replace it.";
        }
        return saveProject(path.string());
    });
}

Component RsansApp::makeQuitPrompt() {
    Component saveFirst = Button("Save", [this] {
        d_quitPromptOpen = false;
        d_afterSave = d_promptLeave;
        saveOrAsk();  // Leaves once saved (if it asks where, cancelling stays)
    }, ButtonOption::Ascii());
    Component discard = Button("Don't save", [this] {
        d_quitPromptOpen = false;
        leave(d_promptLeave);
    }, ButtonOption::Ascii());
    Component cancel = Button("Cancel", [this] { d_quitPromptOpen = false; }, ButtonOption::Ascii());
    Component buttons = Container::Horizontal({saveFirst, discard, cancel});

    Component rendered = Renderer(buttons, [this, saveFirst, discard, cancel] {
        const std::string name = d_projectPath.empty()
            ? "This project"
            : std::filesystem::path(d_projectPath).filename().string();
        const std::string question = d_promptLeave == Leave::Menu ? "Save before going back to the menu?"
                                                                  : "Save before quitting?";
        return dialogWindow(" Unsaved changes ", vbox({
            text(name + " has unsaved changes."),
            text(question),
            text(""),
            hbox({saveFirst->Render(), text(" "), discard->Render(), text(" "), cancel->Render()}),
        }));
    });
    return CatchEvent(rendered, [this](const Event& event) {
        if (event == Event::Escape) {
            d_quitPromptOpen = false;
            return true;
        }
        return false;
    });
}

RsansApp::~RsansApp() {
    if (d_fontScan.joinable()) {
        d_fontScan.join();
    }
}

void RsansApp::refreshWhisperModels() {
    d_models = listWhisperModels(whisperDir);
    refreshModelList();
}

void RsansApp::refreshModelList() {
    // The main models, or every one
    d_modelIndexes.clear();
    if (d_modelShowAll) {
        for (std::size_t i = 0; i < d_models.size(); ++i) {
            d_modelIndexes.push_back(i);
        }
    } else {
        d_modelIndexes = mainWhisperModels(d_models);
    }

    // Names in a column, then "✓" or the download size
    std::size_t nameWidth = 0;
    for (const std::size_t index : d_modelIndexes) {
        nameWidth = std::max(nameWidth, d_models[index].name.size());
    }
    d_modelLabels.clear();
    for (const std::size_t index : d_modelIndexes) {
        const WhisperModel& model = d_models[index];
        const std::string mark = modelLabel(model).substr(model.name.size() + 1);
        d_modelLabels.push_back(model.name + std::string(nameWidth - model.name.size() + 2, ' ') + mark);
    }

    // Keep the chosen model highlighted
    d_modelSelected = 0;
    for (std::size_t i = 0; i < d_modelIndexes.size(); ++i) {
        if (d_models[d_modelIndexes[i]].path == d_form.project().model.base) {
            d_modelSelected = static_cast<int>(i);
        }
    }
}

void RsansApp::openModelPicker() {
    d_modelView = 0;
    d_modelToDownload.reset();
    refreshWhisperModels();
    d_modelPickerOpen = true;
}

std::optional<std::string> RsansApp::modelAwaitingDownload() const {
    if (d_modelPickerOpen && d_modelView == 1 && d_modelToDownload && *d_modelToDownload < d_models.size()) {
        return d_models[*d_modelToDownload].name;
    }
    return std::nullopt;
}

void RsansApp::chooseSelectedModel() {
    const auto row = static_cast<std::size_t>(std::max(d_modelSelected, 0));
    if (row >= d_modelIndexes.size()) {
        return;
    }
    const std::size_t index = d_modelIndexes[row];
    if (d_models[index].installed) {
        d_form.setFieldText("model.path", d_models[index].path);
        d_modelPickerOpen = false;
        return;
    }
    // Not installed: ask before downloading
    d_modelToDownload = index;
    d_modelView = 1;
}

bool RsansApp::downloadModel(const std::string& name) {
    const auto found = std::find_if(d_models.begin(), d_models.end(),
                                    [&](const WhisperModel& model) { return model.name == name; });
    if (found == d_models.end() || d_runner.isRunning()) {
        return false;
    }
    const std::string script = whisperDownloadScript(whisperDir);
    const std::string modelsDir = whisperModelsDir(whisperDir);
    const std::string target = found->path;

    return d_runner.start("Download " + name, [script, modelsDir, name, target](const StepContext& context) {
        context.onStage("Downloading " + name);
        // The script skips models that already exist, so clear out an empty file
        std::error_code ec;
        if (std::filesystem::exists(target, ec) && std::filesystem::file_size(target, ec) == 0) {
            std::filesystem::remove(target, ec);
        }

        try {
            // Its own process group, so cancelling also stops the curl or wget it runs
            const int exitCode = runProcess(
                {"sh", script, name, modelsDir},
                [&context](const std::string& line) {
                    const std::optional<double> progress = parseDownloadProgress(line);
                    if (progress) {
                        context.onProgress(*progress);
                    } else if (line.find_first_not_of(" 	") != std::string::npos) {
                        context.onLog(line);
                    }
                },
                context.cancel, 3000, true);
            if (exitCode != 0) {
                throw std::runtime_error("whisper.cpp's download script failed (exit code " +
                                         std::to_string(exitCode) + "); see the log");
            }
            if (!std::filesystem::exists(target, ec) || std::filesystem::file_size(target, ec) == 0) {
                throw std::runtime_error("The download finished but " + target + " is missing or empty");
            }
        } catch (...) {
            // Don't leave a partial model behind: it would look installed
            std::filesystem::remove(target, ec);
            throw;
        }

        context.onProgress(1.0);
        context.onLog("Saved " + target);
        ProjectData result;
        result.model.base = target;
        return result;
    }, [this](const ProjectData& result) {
        d_form.setFieldText("model.path", result.model.base);
        refreshWhisperModels();
    });
}

Component RsansApp::makeModelPicker() {
    // The list
    MenuOption listOption = MenuOption::Vertical();
    listOption.on_enter = [this] { chooseSelectedModel(); };
    Component list = Menu(&d_modelLabels, &d_modelSelected, listOption);

    CheckboxOption showAllOption = CheckboxOption::Simple();
    showAllOption.on_change = [this] { refreshModelList(); };
    Component showAll = Checkbox("Show all", &d_modelShowAll, showAllOption);
    Component otherFile = Button("Other file...", [this] {
        openFileDialog("Whisper model", {".bin"}, false, d_form.project().model.base,
                       [this](const std::string& path) {
                           d_form.setFieldText("model.path", path);
                           d_modelPickerOpen = false;
                           return std::string();
                       });
    }, ButtonOption::Ascii());
    Component listView = Container::Vertical({list, Container::Horizontal({showAll, otherFile})});

    // Confirming a download
    Component download = Button("Download", [this] {
        if (d_modelToDownload && *d_modelToDownload < d_models.size() &&
            downloadModel(d_models[*d_modelToDownload].name)) {
            d_modelPickerOpen = false;
        }
    }, ButtonOption::Ascii());
    Component back = Button("Back", [this] { d_modelView = 0; }, ButtonOption::Ascii());
    Component confirmView = Container::Horizontal({download, back});

    Component views = Container::Tab({listView, confirmView}, &d_modelView);

    Component rendered = Renderer(views, [this, list, showAll, otherFile, download, back] {
        Element body;
        if (d_modelView == 1 && d_modelToDownload && *d_modelToDownload < d_models.size()) {
            const WhisperModel& model = d_models[*d_modelToDownload];
            const std::string size = model.sizeBytes
                ? " (" + std::string(model.sizeEstimated ? "about " : "") + formatSize(*model.sizeBytes) + ")"
                : "";
            body = vbox({
                text("Download " + model.name + size + "?") | bold,
                paragraph("Saved to " + whisperModelsDir(whisperDir)) | dim,
                d_runner.isRunning()
                    ? text("Wait for " + d_runner.name() + " to finish first") | color(ftxui::Color::Red)
                    : text("Progress shows on the Run tab") | dim,
                text(""),
                hbox({d_runner.isRunning() ? download->Render() | dim : download->Render(), text(" "),
                      back->Render()}),
            });
        } else {
            Element listing = d_models.empty()
                ? paragraph("No models found. Set WHISPER_CPP_DIR to a whisper.cpp checkout.") |
                      color(ftxui::Color::Red)
                : list->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 16);
            body = vbox({
                listing,
                separator(),
                hbox({showAll->Render(), text("  "), otherFile->Render(), filler(),
                      text("\u2713 installed") | dim}),
            });
        }
        return dialogWindow(" Whisper model ", body) | size(WIDTH, EQUAL, 50);
    });

    return CatchEvent(rendered, [this](const Event& event) {
        if (event == Event::Escape) {
            if (d_modelView == 1) {
                d_modelView = 0;
            } else {
                d_modelPickerOpen = false;
            }
            return true;
        }
        return false;
    });
}

void RsansApp::useSettingsFile(const std::string& path, bool ffmpegGiven) {
    d_settingsPath = path;
    d_settings = loadSettings(path);
    if (!ffmpegGiven && !d_settings.ffmpegPath.empty()) {
        ffmpegPath = d_settings.ffmpegPath;
    }
    const char* whisperOverride = std::getenv("WHISPER_CPP_DIR");
    const bool whisperGiven = whisperOverride != nullptr && *whisperOverride != '\0';
    if (!whisperGiven && !d_settings.whisperDir.empty()) {
        whisperDir = d_settings.whisperDir;
    }
    refreshWhisperModels();
}

void RsansApp::settingsChanged(Setting edited) {
    if (d_settingsPath.empty()) {
        return;
    }
    // Only the edited value is saved: a --ffmpeg or $WHISPER_CPP_DIR given for
    // one run, or a default, isn't written unless changed here
    if (edited == Setting::Ffmpeg) {
        d_settings.ffmpegPath = ffmpegPath;
    } else {
        d_settings.whisperDir = whisperDir;
    }
    d_settingsError = saveSettings(d_settingsPath, d_settings);
}

void RsansApp::newProject() {
    d_form.reset(defaultProject());
    d_lastVideoFolder.clear();
    d_projectPath.clear();
    d_saveMessage.clear();
    d_wordSelected = 0;
    d_rhymesOutOfDate = false;
    markSaved();
    d_tab = 0;
    d_screen = static_cast<int>(Screen::Editor);
}

std::string RsansApp::loadProject(const std::string& path) {
    try {
        ProjectData project(readFile(path));
        d_form.reset(std::move(project));
    } catch (const std::exception& e) {
        return std::string("Can't load project: ") + e.what();
    }
    d_projectPath = path;
    d_lastVideoFolder.clear();
    d_saveMessage.clear();
    d_wordSelected = 0;
    d_rhymesOutOfDate = false;
    markSaved();
    d_tab = 0;
    d_screen = static_cast<int>(Screen::Editor);
    return "";
}

void RsansApp::chooseStartEntry() {
    switch (d_startSelected) {
    case 0:
        newProject();
        break;
    case 1:
        openFileDialog("Load project", {projectExtension, ".json"}, false, "",
                       [this](const std::string& path) { return loadProject(path); });
        break;
    case 2:
        d_screen = static_cast<int>(Screen::Credits);
        break;
    default:
        d_quit();
        break;
    }
}

Component RsansApp::makeStartScreen() {
    MenuOption option = MenuOption::Vertical();
    option.on_enter = [this] { chooseStartEntry(); };
    Component menu = Menu(&d_startEntries, &d_startSelected, option);

    return Renderer(menu, [menu] {
        // The block-letter title needs a wide terminal; otherwise plain text
        const bool fits = Terminal::Size().dimx >= titleArtWidth + 2;
        Element title = fits ? titleArt() : text("R S A N S") | bold | color(ftxui::Color::BlueLight);
        return vbox({
            filler(),
            title | hcenter,
            text(""),
            text("Lyric Alignment & Visualization Engine") | dim | hcenter,
            text(""),
            text(""),
            menu->Render() | size(WIDTH, GREATER_THAN, 20) | border | hcenter,
            filler(),
            text("Up/Down: choose   Enter: select   Ctrl+C: quit") | dim | hcenter,
        });
    });
}

Component RsansApp::makeCredits() {
    Component back = Button("Back", [this] { d_screen = static_cast<int>(Screen::Start); },
                            ButtonOption::Ascii());

    Component rendered = Renderer(back, [back] {
        struct Credit {
            const char* name;
            const char* use;
            const char* license;
        };
        static const std::array<Credit, 11> credits = {{
            {"whisper.cpp", "Speech recognition and word timing", "MIT"},
            {"CMU Pronouncing Dictionary", "Pronunciations for rhyme detection", "BSD-2-Clause"},
            {"FFmpeg", "Audio conversion and video rendering", "LGPL/GPL"},
            {"libass", "Subtitle rendering", "ISC"},
            {"libsndfile", "Reading audio files", "LGPL-2.1"},
            {"fontconfig", "Finding installed fonts", "MIT-style"},
            {"FTXUI", "This terminal interface", "MIT"},
            {"CLI11", "Command line parsing", "BSD-3-Clause"},
            {"nlohmann/json", "Project files", "MIT"},
            {"dr_libs", "Audio decoding", "Public domain / MIT-0"},
            {"stb", "Image reading", "Public domain / MIT"},
        }};
        Elements rows;
        for (const Credit& credit : credits) {
            rows.push_back(hbox({
                text(credit.name) | bold | size(WIDTH, EQUAL, 30),
                text(credit.use) | size(WIDTH, EQUAL, 40),
                text(credit.license) | dim,
            }));
        }
        return vbox({
            filler(),
            text("RSANS Lyric Alignment & Visualization Engine") | bold | hcenter,
            text(""),
            text("Built with") | dim | hcenter,
            text(""),
            vbox(std::move(rows)) | hcenter,
            text(""),
            back->Render() | hcenter,
            filler(),
            text("Enter or Esc: back   Ctrl+C: quit") | dim | hcenter,
        });
    });

    return CatchEvent(rendered, [this](const Event& event) {
        if (event == Event::Escape) {
            d_screen = static_cast<int>(Screen::Start);
            return true;
        }
        return false;
    });
}

void RsansApp::scanFonts() {
    if (d_fontsScanning) {
        return;
    }
    if (d_fontScan.joinable()) {
        d_fontScan.join();  // The previous scan has finished
    }
    d_fontsScanning = true;
    refreshFontMatches();
    d_fontScan = std::thread([this] {
        std::vector<FontInfo> fonts = scanSystemFonts();
        d_post([this, fonts = std::move(fonts)]() mutable {
            d_fontsScanning = false;
            setFonts(std::move(fonts));
        });
    });
}

void RsansApp::setFonts(std::vector<FontInfo> fonts) {
    d_fonts = std::move(fonts);
    refreshFontMatches();
}

void RsansApp::openFontPicker(FontTarget target) {
    d_pickerTarget = target;
    fontQuery.clear();
    refreshFontMatches();

    // Start on the font already chosen, if it's in the list
    const std::string currentFamily = target == FontTarget::Header
        ? d_form.project().header.fontName
        : d_form.project().layout.fontName;
    for (std::size_t i = 0; i < d_matchIndexes.size(); ++i) {
        if (d_fonts[d_matchIndexes[i]].family == currentFamily) {
            d_matchSelected = static_cast<int>(i);
            break;
        }
    }
    d_pickerOpen = true;
}

void RsansApp::refreshFontMatches() {
    // A path to a font file can be used directly, installed or not
    d_fileMatch.reset();
    if (fontQuery.find('/') != std::string::npos) {
        d_fileMatch = fontFromFile(fontQuery);
    }

    d_matchIndexes = filterFonts(d_fonts, fontQuery);
    d_matchLabels.clear();
    if (d_fileMatch) {
        d_matchLabels.push_back("Use file: " + d_fileMatch->family + " (" + d_fileMatch->style + ")");
    }
    for (const std::size_t index : d_matchIndexes) {
        d_matchLabels.push_back(d_fonts[index].family);
    }
    d_matchSelected = 0;
}

void RsansApp::chooseSelectedFont() {
    if (d_matchLabels.empty()) {
        return;
    }
    std::size_t selected = static_cast<std::size_t>(std::max(d_matchSelected, 0));
    if (d_fileMatch) {
        if (selected == 0) {
            chooseFont(*d_fileMatch);
            return;
        }
        --selected;
    }
    if (selected < d_matchIndexes.size()) {
        chooseFont(d_fonts[d_matchIndexes[selected]]);
    }
}

void RsansApp::chooseFont(const FontInfo& font) {
    const FontKeys keys = fontKeys(d_pickerTarget);
    d_form.setFieldText(keys.name, font.family);
    d_form.setFieldText(keys.path, font.path);
    d_pickerOpen = false;
}

Component RsansApp::makeFontPicker() {
    InputOption searchOption = singleLineInput();
    searchOption.on_change = [this] { refreshFontMatches(); };
    searchOption.on_enter = [this] { chooseSelectedFont(); };
    Component search = TextInput(&fontQuery, "type to search, or a path to a font file", searchOption);

    MenuOption listOption = MenuOption::Vertical();
    listOption.on_enter = [this] { chooseSelectedFont(); };
    Component list = Menu(&d_matchLabels, &d_matchSelected, listOption);

    Component rescan = Button("Rescan fonts", [this] { scanFonts(); }, ButtonOption::Ascii());
    Component browse = Button("Browse files", [this] {
        const ProjectData& project = d_form.project();
        const std::string& current = d_pickerTarget == FontTarget::Header ? project.header.fontPath
                                                                           : project.layout.fontPath;
        openFileDialog("Font file", fontTypes, false, current, [this](const std::string& path) {
            const std::optional<FontInfo> font = fontFromFile(path);
            if (!font) {
                return std::string("Not a font file this program can read");
            }
            chooseFont(*font);
            return std::string();
        });
    }, ButtonOption::Ascii());
    Component cancel = Button("Cancel", [this] { d_pickerOpen = false; }, ButtonOption::Ascii());

    Component container =
        Container::Vertical({search, list, Container::Horizontal({rescan, browse, cancel})});

    Component rendered = Renderer(container, [this, search, list, rescan, browse, cancel] {
        const std::string title = d_pickerTarget == FontTarget::Header ? " Header font " : " Lyrics font ";

        std::string status;
        if (d_fontsScanning) {
            status = "Scanning installed fonts...";
        } else if (d_fonts.empty()) {
            status = "No fonts found. Type a path to a font file, or rescan.";
        } else {
            status = std::to_string(d_matchIndexes.size()) + " of " +
                     std::to_string(d_fonts.size()) + " fonts";
        }

        // Where the highlighted font comes from
        std::string selectedPath;
        const auto selected = static_cast<std::size_t>(std::max(d_matchSelected, 0));
        if (d_fileMatch && selected == 0) {
            selectedPath = d_fileMatch->path;
        } else {
            const std::size_t index = selected - (d_fileMatch ? 1 : 0);
            if (index < d_matchIndexes.size()) {
                selectedPath = d_fonts[d_matchIndexes[index]].path;
            }
        }

        return dialogWindow(title, vbox({
            hbox({text("Search ") | bold, search->Render() | flex}),
            text(status) | dim,
            separator(),
            list->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 14),
            separator(),
            text(selectedPath.empty() ? " " : selectedPath) | dim,
            hbox({rescan->Render(), text(" "), browse->Render(), text(" "), cancel->Render(), filler(),
                  text("Enter: choose  Esc: close") | dim}),
        })) | size(WIDTH, EQUAL, 72);
    });

    return CatchEvent(rendered, [this](const Event& event) {
        if (event == Event::Escape) {
            d_pickerOpen = false;
            return true;
        }
        return false;
    });
}

void RsansApp::openFileDialog(std::string title, std::vector<std::string> extensions, bool saveMode,
                              const std::string& current, FileChosen onChoose) {
    d_browser.emplace(std::move(extensions), saveMode, current);
    d_fileTitle = std::move(title);
    d_onFileChosen = std::move(onChoose);
    d_showAllFiles = false;
    fileQuery.clear();
    d_fileError.clear();
    refreshFileLabels();
    d_fileDialogOpen = true;
}

void RsansApp::browseFor(const std::string& key) {
    if (key == "ffmpeg") {
        openFileDialog("FFmpeg program", {}, false, ffmpegPath, [this](const std::string& path) {
            ffmpegPath = path;
            settingsChanged(Setting::Ffmpeg);
            return std::string();
        });
        return;
    }

    std::vector<std::string> types;
    if (key == "header.media") {
        types = imageTypes;
    } else if (key == "audio.path") {
        types = audioTypes;
    } else if (key == "model.path") {
        types = modelTypes;
    } else if (key == "cmudict") {
        types = dictTypes;
    } else if (key == "header.fontPath" || key == "layout.fontPath") {
        types = fontTypes;
    }
    std::string title = key;
    std::string current;
    for (const FormField& field : d_form.fields()) {
        if (field.key == key) {
            title = field.label;
            current = field.text;
        }
    }
    openFileDialog(title, types, false, current, [this, key](const std::string& path) {
        d_form.setFieldText(key, path);
        return std::string();
    });
}

void RsansApp::refreshFileLabels() {
    if (!d_browser) {
        return;
    }
    // When the browser changed the text (a starting file name, or cleared on
    // entering a folder), put the cursor at its end, ready to type
    if (fileQuery != d_browser->query()) {
        fileQuery = d_browser->query();
        d_fileCursor = static_cast<int>(fileQuery.size());
    }
    d_fileLabels.clear();
    for (const FileBrowser::Entry& entry : d_browser->entries()) {
        d_fileLabels.push_back(entry.isDirectory ? entry.name + "/" : entry.name);
    }
    // Highlight the first real entry rather than ".."
    const std::vector<FileBrowser::Entry>& entries = d_browser->entries();
    d_fileSelected = entries.size() > 1 && entries.front().isParent ? 1 : 0;
}

void RsansApp::finishFileAction(const std::optional<std::string>& chosen) {
    d_fileError.clear();
    if (chosen) {
        const std::string error = d_onFileChosen ? d_onFileChosen(*chosen) : std::string();
        if (error.empty()) {
            d_fileDialogOpen = false;
            return;
        }
        d_fileError = error;
    }
    refreshFileLabels();
}

Component RsansApp::makeFileDialog() {
    InputOption searchOption = singleLineInput();
    searchOption.on_change = [this] {
        if (d_browser) {
            d_browser->setQuery(fileQuery);
            d_fileError.clear();
            refreshFileLabels();
        }
    };
    searchOption.on_enter = [this] {
        if (d_browser) {
            finishFileAction(d_browser->submitQuery());
        }
    };
    searchOption.cursor_position = &d_fileCursor;
    Component search = TextInput(&fileQuery, "", searchOption);

    MenuOption listOption = MenuOption::Vertical();
    listOption.on_enter = [this] {
        if (d_browser) {
            finishFileAction(d_browser->activate(static_cast<std::size_t>(std::max(d_fileSelected, 0))));
        }
    };
    Component list = Menu(&d_fileLabels, &d_fileSelected, listOption);

    CheckboxOption showAllOption = CheckboxOption::Simple();
    showAllOption.on_change = [this] {
        if (d_browser) {
            d_browser->setShowAllFiles(d_showAllFiles);
            refreshFileLabels();
        }
    };
    Component showAll = Maybe(Checkbox("Show all files", &d_showAllFiles, showAllOption),
                              [this] { return d_browser && d_browser->hasExtensionFilter(); });
    Component up = Button("Up", [this] {
        if (d_browser) {
            d_browser->goUp();
            refreshFileLabels();
        }
    }, ButtonOption::Ascii());
    Component cancel = Button("Cancel", [this] { d_fileDialogOpen = false; }, ButtonOption::Ascii());

    Component container =
        Container::Vertical({search, list, Container::Horizontal({showAll, up, cancel})});

    Component rendered = Renderer(container, [this, search, list, showAll, up, cancel] {
        if (!d_browser) {
            return text("");
        }
        const FileBrowser& browser = *d_browser;

        // What's listed, or what went wrong
        Element status;
        if (!d_fileError.empty()) {
            status = text(d_fileError) | color(ftxui::Color::Red);
        } else if (!browser.error().empty()) {
            status = text(browser.error()) | color(ftxui::Color::Red);
        } else if (browser.hasExtensionFilter() && !browser.showAllFiles()) {
            std::string types;
            for (const std::string& extension : browser.extensions()) {
                types += (types.empty() ? "" : " ") + extension;
            }
            status = text("Showing folders and " + types + " files") | dim;
        } else {
            status = text("Showing all files") | dim;
        }

        Element listing = d_fileLabels.empty() ? text("(nothing here)") | dim
                                               : list->Render() | vscroll_indicator | frame;
        const std::string hint = browser.saveMode() ? "Enter: open folder / save  Esc: close"
                                                    : "Enter: open  Backspace: up  Esc: close";
        return dialogWindow(" " + d_fileTitle + " ", vbox({
            hbox({text("Folder ") | bold, text(elideLeft(browser.directory().string(), 70)) | dim}),
            hbox({text(browser.saveMode() ? "Name   " : "Search ") | bold, search->Render() | flex}),
            status,
            separator(),
            listing | size(HEIGHT, LESS_THAN, 16),
            separator(),
            hbox({showAll->Render(), text(" "), up->Render(), text(" "), cancel->Render(), filler(),
                  text(hint) | dim}),
        })) | size(WIDTH, EQUAL, 80);
    });

    return CatchEvent(rendered, [this](const Event& event) {
        if (event == Event::Escape) {
            d_fileDialogOpen = false;
            return true;
        }
        // Backspace with nothing typed goes up a folder (opening files only;
        // when saving, the box holds the file name)
        if (event == Event::Backspace && fileQuery.empty() && d_browser && !d_browser->saveMode()) {
            d_browser->goUp();
            refreshFileLabels();
            return true;
        }
        return false;
    });
}

Component RsansApp::browseButton(const std::string& key) {
    return Button("Browse", [this, key] { browseFor(key); }, ButtonOption::Ascii());
}

Component RsansApp::makeProjectTab() {
    struct Row {
        FormField* field;
        Component input;
        Component browse;  // Null for fields that aren't paths
    };
    Components inputs;
    std::vector<Row> rows;
    for (FormField& field : d_form.fields()) {
        // Each font is one row: a button opening the picker, which fills in
        // both the font's name and its file
        const bool isFontName = field.key == "header.fontName" || field.key == "layout.fontName";
        const bool isFontPath = field.key == "header.fontPath" || field.key == "layout.fontPath";
        if (isFontPath) {
            continue;
        }
        if (field.key == "model.path") {
            Component button = Button(&d_modelButtonLabel, [this] { openModelPicker(); }, ButtonOption::Ascii());
            inputs.push_back(button);
            rows.push_back({&field, button, nullptr});
            continue;
        }
        if (isFontName) {
            const FontTarget target = field.key == "header.fontName" ? FontTarget::Header : FontTarget::Layout;
            std::string* label = target == FontTarget::Header ? &d_headerFontLabel : &d_layoutFontLabel;
            Component button = Button(label, [this, target] { openFontPicker(target); }, ButtonOption::Ascii());
            inputs.push_back(button);
            rows.push_back({&field, button, nullptr});
            continue;
        }

        InputOption option = singleLineInput();
        option.on_change = [this] { d_form.apply(); };
        option.placeholder = field.kind == FieldKind::Path ? "path to file" : "";
        Component input = TextInput(&field.text, option);
        if (field.kind == FieldKind::Path) {
            // Typed, or picked with the file dialog
            Component browse = browseButton(field.key);
            inputs.push_back(Container::Horizontal({input, browse}));
            rows.push_back({&field, input, browse});
        } else {
            inputs.push_back(input);
            rows.push_back({&field, input, nullptr});
        }
    }

    Component container = Container::Vertical(inputs);
    return Renderer(container, [this, rows] {
        Elements lines;
        std::string section;
        for (const Row& entry : rows) {
            FormField* const field = entry.field;
            const Component& input = entry.input;
            if (field->section != section) {
                section = field->section;
                if (!lines.empty()) {
                    lines.push_back(text(""));
                }
                lines.push_back(text(section) | bold);
            }
            std::vector<std::string> errors = d_form.errorsFor(field->key);

            // The model row: its name if it's one of whisper.cpp's, a running
            // download, and the file
            if (field->key == "model.path") {
                const std::string& path = d_form.project().model.base;
                const auto known = std::find_if(d_models.begin(), d_models.end(),
                                                [&](const WhisperModel& m) { return m.path == path; });
                d_modelButtonLabel = path.empty()      ? "Choose a model..."
                                     : known != d_models.end() ? known->name
                                                               : std::filesystem::path(path).filename().string();
                Element status = text("");
                if (d_runner.isRunning() && d_runner.name().rfind("Download ", 0) == 0) {
                    status = text("  " + d_runner.name() + "... " + percent(d_runner.progress())) |
                             color(ftxui::Color::YellowLight);
                }
                lines.push_back(hbox({
                    text("  " + field->label) | size(WIDTH, EQUAL, labelWidth),
                    text(errors.empty() ? "  " : "! ") | color(ftxui::Color::Red),
                    input->Render(),
                    status,
                    text("  " + path) | dim,
                }));
                for (const std::string& error : errors) {
                    if (error != requiredMessage) {
                        lines.push_back(text(std::string(labelWidth + 2, ' ') + error) | color(ftxui::Color::Red));
                    }
                }
                continue;
            }

            // Font rows: show the chosen family and file, and the file's errors too
            const bool isHeaderFont = field->key == "header.fontName";
            if (isHeaderFont || field->key == "layout.fontName") {
                const FontKeys keys = fontKeys(isHeaderFont ? FontTarget::Header : FontTarget::Layout);
                const std::vector<std::string> pathErrors = d_form.errorsFor(keys.path);
                errors.insert(errors.end(), pathErrors.begin(), pathErrors.end());
                // A missing font is one problem, not two
                if (std::count(errors.begin(), errors.end(), requiredMessage) > 1) {
                    errors.erase(std::find(errors.begin(), errors.end(), requiredMessage));
                }
                const ProjectData& project = d_form.project();
                const std::string& family = isHeaderFont ? project.header.fontName : project.layout.fontName;
                const std::string& path = isHeaderFont ? project.header.fontPath : project.layout.fontPath;
                std::string& label = isHeaderFont ? d_headerFontLabel : d_layoutFontLabel;
                label = family.empty() ? "Choose a font..." : family;
                Element fontRow = hbox({
                    text("  " + field->label) | size(WIDTH, EQUAL, labelWidth),
                    text(errors.empty() ? "  " : "! ") | color(ftxui::Color::Red),
                    input->Render(),
                    text("  " + path) | dim,
                });
                lines.push_back(fontRow);
                for (const std::string& error : errors) {
                    if (error != requiredMessage) {
                        lines.push_back(text(std::string(labelWidth + 2, ' ') + error) | color(ftxui::Color::Red));
                    }
                }
                const std::string warning = d_form.warningFor(field->key);
                if (!warning.empty()) {
                    lines.push_back(hbox({text(std::string(labelWidth + 2, ' ')),
                                          paragraph(warning) | color(ftxui::Color::Yellow)}));
                }
                continue;
            }
            Element row = hbox({
                text("  " + field->label) | size(WIDTH, EQUAL, labelWidth),
                text(errors.empty() ? "  " : "! ") | color(ftxui::Color::Red),
                input->Render() | flex,
                entry.browse ? hbox({text(" "), entry.browse->Render()}) : text(""),
            });
            lines.push_back(row);
            // A missing value is shown by the "!" alone (explained once at the
            // bottom); other problems, like a bad file, are spelled out
            for (const std::string& error : errors) {
                if (error != requiredMessage) {
                    lines.push_back(text(std::string(labelWidth + 2, ' ') + error) | color(ftxui::Color::Red));
                }
            }
            if (field->key == "audio.path" && d_form.project().audio.length > 0.0) {
                const int ms = static_cast<int>(d_form.project().audio.length * 1000.0);
                lines.push_back(text(std::string(labelWidth + 2, ' ') + "Length " + formatTime(ms)) | dim);
            }
        }
        return vbox({
            vbox(std::move(lines)) | yframe | vscroll_indicator | flex,
            separator(),
            hbox({text("! ") | color(ftxui::Color::Red), text("= value is required") | dim}),
        });
    });
}

std::vector<std::string> RsansApp::problemsFor(Step step) const {
    const ProjectData& project = d_form.project();
    std::vector<std::string> problems;
    switch (step) {
    case Step::Analyze: problems = describe(project.validateForAnalyze()); break;
    case Step::Rhyme:   problems = describe(project.validateForRhyme()); break;
    case Step::Export:  problems = describe(project.validateForExport()); break;
    case Step::Full:    problems = describe(project.validateForFull()); break;
    }

    const bool needsFfmpeg = step != Step::Rhyme;
    if (needsFfmpeg && ffmpegPath.empty()) {
        problems.emplace_back("ffmpeg: is required");
    }
    // (Where to save the video is asked when Export or Full run starts)
    return problems;
}

std::string RsansApp::videoPathSuggestion() const {
    std::filesystem::path folder;
    if (!d_lastVideoFolder.empty()) {
        folder = d_lastVideoFolder;
    } else if (!d_projectPath.empty()) {
        folder = std::filesystem::absolute(d_projectPath).parent_path();
    } else {
        folder = std::filesystem::current_path();
    }
    std::string name = d_projectPath.empty() ? fileNameFrom(d_form.project().header.title)
                                             : std::filesystem::path(d_projectPath).stem().string();
    if (name.empty()) {
        name = "video";
    }
    return (folder / (name + ".mp4")).string();
}

void RsansApp::askWhereToSaveVideo(Step step) {
    d_overwriteCandidate.clear();
    openFileDialog("Save video as", videoTypes, true, videoPathSuggestion(), [this, step](const std::string& chosen) {
        std::filesystem::path path(chosen);
        if (!path.has_extension()) {
            path += ".mp4";
        }
        if (path.extension() == ".ass" || path.extension() == ".rsans") {
            return "The video can't be a " + path.extension().string() + " file (choose a name ending in .mp4)";
        }
        // Replacing another file needs a second Enter
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && path.string() != d_overwriteCandidate) {
            d_overwriteCandidate = path.string();
            return path.filename().string() + " already exists. Press Enter again to replace it.";
        }
        if (!startVideoStep(step, path.string())) {
            return std::string("Can't start: check the Run tab");
        }
        return std::string();
    });
}

bool RsansApp::runStep(Step step) {
    if (step == Step::Export || step == Step::Full) {
        if (d_runner.isRunning() || !problemsFor(step).empty()) {
            return false;
        }
        askWhereToSaveVideo(step);
        return true;
    }
    return startVideoStep(step, "");
}

bool RsansApp::startVideoStep(Step step, const std::string& outputPath) {
    if (d_runner.isRunning() || !problemsFor(step).empty()) {
        return false;
    }
    const bool writesVideo = step == Step::Export || step == Step::Full;
    if (writesVideo) {
        if (outputPath.empty()) {
            return false;
        }
        try {
            subtitlePathFor(outputPath);
        } catch (const std::invalid_argument&) {
            return false;
        }
        d_lastVideoFolder = std::filesystem::absolute(outputPath).parent_path().string();
    }

    const ProjectData project = d_form.project();
    const std::string ffmpeg = ffmpegPath;
    const std::string output = outputPath;
    const auto keepTokens = [this](ProjectData result) {
        d_form.setTokens(std::move(result.tokens));
        d_wordSelected = 0;
        d_rhymesOutOfDate = false;  // New words, or freshly detected rhymes
    };

    switch (step) {
    case Step::Analyze:
        return d_runner.start("Analyze", [project, ffmpeg](const StepContext& context) {
            context.onStage("Transcribing");
            return tokenizeAudio(project, ffmpeg, whisperLog(context), context.onProgress, context.cancel);
        }, keepTokens);

    case Step::Rhyme:
        return d_runner.start("Detect rhymes", [project](const StepContext& context) {
            context.onStage("Detecting rhymes");
            ProjectData result = detectRhymes(project);
            context.onProgress(1.0);
            return result;
        }, keepTokens);

    case Step::Export:
        return d_runner.start("Export", [project, ffmpeg, output](const StepContext& context) {
            context.onStage("Rendering video");
            exportVideo(project, output, ffmpeg, context.onLog, context.onProgress, context.cancel);
            context.onLog("Saved " + output);
            return project;
        }, nullptr);

    case Step::Full:
        return d_runner.start("Full run", [project, ffmpeg, output](const StepContext& context) {
            context.onStage("Transcribing");
            const ProjectData analyzed = tokenizeAudio(project, ffmpeg, whisperLog(context),
                                                       scaledProgress(context.onProgress, 0.0, 0.7),
                                                       context.cancel);
            throwIfCancelled(context.cancel, "Cancelled");
            context.onStage("Detecting rhymes");
            ProjectData rhymed = detectRhymes(analyzed);
            context.onStage("Rendering video");
            exportVideo(rhymed, output, ffmpeg, context.onLog,
                        scaledProgress(context.onProgress, 0.7, 1.0), context.cancel);
            context.onLog("Saved " + output);
            return rhymed;
        }, keepTokens);
    }
    return false;
}

Component RsansApp::makeRunTab() {
    // Settings for this computer, saved for next time
    InputOption ffmpegOption = singleLineInput();
    ffmpegOption.on_change = [this] { settingsChanged(Setting::Ffmpeg); };
    Component ffmpegInput = TextInput(&ffmpegPath, "ffmpeg", ffmpegOption);
    Component ffmpegBrowse = browseButton("ffmpeg");
    InputOption whisperOption = singleLineInput();
    whisperOption.on_change = [this] {
        settingsChanged(Setting::Whisper);
        refreshWhisperModels();
    };
    Component whisperInput = TextInput(&whisperDir, "path to a whisper.cpp checkout", whisperOption);

    struct StepButton {
        Step step;
        std::string label;
        Component button;
    };
    std::vector<StepButton> steps;
    for (const auto& [step, label] : std::vector<std::pair<Step, std::string>>{
             {Step::Analyze, "Analyze"}, {Step::Rhyme, "Detect rhymes"},
             {Step::Export, "Export..."}, {Step::Full, "Full run..."}}) {
        const Step thisStep = step;
        steps.push_back({step, label, Button(label, [this, thisStep] { runStep(thisStep); },
                                             ButtonOption::Ascii())});
    }
    Component cancelButton = Button("Cancel", [this] { d_runner.cancel(); }, ButtonOption::Ascii());

    Components buttons;
    for (const StepButton& entry : steps) {
        buttons.push_back(entry.button);
    }
    buttons.push_back(cancelButton);

    Component container = Container::Vertical({
        Container::Horizontal({ffmpegInput, ffmpegBrowse}),
        whisperInput,
        Container::Horizontal(buttons),
    });

    return Renderer(container, [this, ffmpegInput, ffmpegBrowse, whisperInput, steps, cancelButton] {
        // Buttons for steps that can't run are dimmed (pressing them does nothing)
        Elements buttonRow;
        Elements checks;
        for (const StepButton& entry : steps) {
            const std::vector<std::string> problems = problemsFor(entry.step);
            const bool canRun = problems.empty() && !d_runner.isRunning();
            buttonRow.push_back(canRun ? entry.button->Render() : entry.button->Render() | dim);

            // Fonts matter only to steps that render the video
            const bool renders = entry.step == Step::Export || entry.step == Step::Full;
            const std::vector<std::string> warnings = renders ? d_form.warnings() : std::vector<std::string>{};
            Element status = problems.empty() && !warnings.empty()
                ? text("ready, but " + warnings.front() + " (see Project)") | color(ftxui::Color::Yellow)
                : problems.empty()
                ? text("ready") | color(ftxui::Color::Green)
                : text(problems.front() + (problems.size() > 1
                           ? "  (+" + std::to_string(problems.size() - 1) + " more)"
                           : "")) | color(ftxui::Color::Red);
            std::string name = entry.label;
            if (name.size() > 3 && name.compare(name.size() - 3, 3, "...") == 0) {
                name.resize(name.size() - 3);  // "Export..." on its button, "Export" here
            }
            checks.push_back(hbox({text("  " + name) | size(WIDTH, EQUAL, labelWidth), status}));
        }
        buttonRow.push_back(text("   "));
        buttonRow.push_back(d_runner.isRunning() ? cancelButton->Render()
                                                 : cancelButton->Render() | dim);

        Elements logLines;
        for (const std::string& line : d_runner.log()) {
            logLines.push_back(text(line));
        }
        if (logLines.empty()) {
            logLines.push_back(text("Nothing run yet") | dim);
        }

        Element status = d_runner.state() == StepRunner::State::Idle
            ? text("Choose a step to run") | dim
            : vbox({
                  hbox({text(d_runner.name() + ": ") | bold, text(stateText(d_runner)),
                        text(d_runner.stage().empty() ? "" : " - " + d_runner.stage()) | dim,
                        filler(), text(percent(d_runner.progress()))}),
                  gauge(static_cast<float>(d_runner.progress())),
                  d_runner.error().empty() ? text("") : paragraph(d_runner.error()) | color(ftxui::Color::Red),
              });

        return vbox({
            hbox({text("Settings") | bold,
                  text(d_settingsPath.empty() ? "" : "  saved for next time") | dim}),
            hbox({text("  FFmpeg") | size(WIDTH, EQUAL, labelWidth), ffmpegInput->Render() | flex,
                  text(" "), ffmpegBrowse->Render()}),
            hbox({text("  whisper.cpp") | size(WIDTH, EQUAL, labelWidth), whisperInput->Render() | flex}),
            d_settingsError.empty() ? text("") : text("  " + d_settingsError) | color(ftxui::Color::Red),
            text("Export and Full run ask where to save the video.") | dim,
            text(""),
            hbox(std::move(buttonRow)),
            text(""),
            text("Checks") | bold,
            vbox(std::move(checks)),
            text(""),
            status,
            text(""),
            window(text("Log"), vbox(std::move(logLines)) | focusPositionRelative(0, 1) | yframe) | flex,
        });
    });
}

std::vector<std::vector<std::size_t>> RsansApp::lyricLines() const {
    // Words grouped by line, lines in order, words in their original order
    std::map<int, std::vector<std::size_t>> byLine;
    const std::vector<Token>& tokens = d_form.project().tokens;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        byLine[tokens[i].lineIndex].push_back(i);
    }
    std::vector<std::vector<std::size_t>> lines;
    for (auto& [lineIndex, words] : byLine) {
        lines.push_back(std::move(words));
    }
    return lines;
}

void RsansApp::selectWord(std::size_t index) {
    const std::size_t count = d_form.project().tokens.size();
    d_wordSelected = count == 0 ? 0 : std::min(index, count - 1);
}

bool RsansApp::openWordEditor() {
    if (d_runner.isRunning() || d_wordSelected >= d_form.project().tokens.size()) {
        return false;
    }
    wordEditText = d_form.project().tokens[d_wordSelected].text;
    d_wordCursor = static_cast<int>(wordEditText.size());
    d_wordError.clear();
    refreshGroupChoices();
    d_wordEditorOpen = true;
    return true;
}

void RsansApp::refreshGroupChoices() {
    // The groups in use, each with its words (other than this one)
    const std::vector<Token>& tokens = d_form.project().tokens;
    std::map<int, std::vector<std::string>> groups;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].rhymeIndex) {
            std::vector<std::string>& words = groups[*tokens[i].rhymeIndex];
            if (i != d_wordSelected &&
                std::find(words.begin(), words.end(), tokens[i].text) == words.end()) {
                words.push_back(tokens[i].text);
            }
        }
    }

    d_groupChoices = {noGroup};
    d_groupLabels = {"None"};
    for (const auto& [group, words] : groups) {
        std::string members;
        for (const std::string& word : words) {
            members += (members.empty() ? "" : ", ") + word;
        }
        if (members.empty()) {
            members = "(only this word)";
        } else if (members.size() > 40) {
            members = members.substr(0, 37) + "...";
        }
        d_groupChoices.push_back(group);
        d_groupLabels.push_back(std::to_string(group + 1) + "  " + members);
    }
    d_groupChoices.push_back(newGroup);
    d_groupLabels.emplace_back("New group");

    // Start on the word's current group
    const std::optional<int> current = tokens[d_wordSelected].rhymeIndex;
    const auto at = std::find(d_groupChoices.begin(), d_groupChoices.end(), current.value_or(noGroup));
    d_groupSelected = static_cast<int>(at - d_groupChoices.begin());
}

void RsansApp::applyWordEdit() {
    const std::string before = d_form.project().tokens[d_wordSelected].text;
    if (!d_form.setTokenText(d_wordSelected, wordEditText)) {
        d_wordError = "Type the word, or use Delete word to remove it";
        return;
    }
    if (d_form.project().tokens[d_wordSelected].text != before) {
        d_rhymesOutOfDate = true;
    }

    // The rhyme group chosen by hand
    const auto choice = static_cast<std::size_t>(std::max(d_groupSelected, 0));
    if (choice < d_groupChoices.size()) {
        const int group = d_groupChoices[choice];
        if (group == noGroup) {
            d_form.setTokenRhyme(d_wordSelected, std::nullopt);
        } else if (group == newGroup) {
            // The lowest number not in use, so colors are reused as little as possible
            int unused = 0;
            while (std::find(d_groupChoices.begin(), d_groupChoices.end(), unused) != d_groupChoices.end()) {
                ++unused;
            }
            d_form.setTokenRhyme(d_wordSelected, unused);
        } else {
            d_form.setTokenRhyme(d_wordSelected, group);
        }
    }
    d_wordEditorOpen = false;
}

void RsansApp::deleteSelectedWord() {
    if (d_form.removeToken(d_wordSelected)) {
        d_rhymesOutOfDate = true;
        selectWord(d_wordSelected);  // The next word, or the new last one
    }
    d_wordEditorOpen = false;
}

Component RsansApp::makeWordEditor() {
    InputOption option = singleLineInput();
    option.cursor_position = &d_wordCursor;
    option.on_change = [this] { d_wordError.clear(); };
    option.on_enter = [this] { applyWordEdit(); };
    Component input = TextInput(&wordEditText, "the word", option);

    // Rhyme groups, each in its color
    MenuOption groupOption = MenuOption::Vertical();
    groupOption.on_enter = [this] { applyWordEdit(); };
    groupOption.entries_option.transform = [this](const EntryState& state) {
        Element mark = text("  ");
        const auto index = static_cast<std::size_t>(state.index);
        const std::vector<::Color>& swatch = d_form.project().colorSwatch;
        if (index < d_groupChoices.size() && d_groupChoices[index] >= 0 && !swatch.empty()) {
            const std::size_t colorIndex = static_cast<std::size_t>(d_groupChoices[index]) % swatch.size();
            mark = text("\u25a0 ") | color(toColor(swatch[colorIndex].hex()));
        } else if (index < d_groupChoices.size() && d_groupChoices[index] == newGroup) {
            mark = text("+ ");
        }
        Element label = text(state.label);
        if (state.active) {
            label = label | bold;
        }
        if (state.focused) {
            label = label | inverted;
        }
        return hbox({text(state.active ? "> " : "  "), mark, label});
    };
    Component groups = Menu(&d_groupLabels, &d_groupSelected, groupOption);

    Component saveButton = Button("Save", [this] { applyWordEdit(); }, ButtonOption::Ascii());
    Component deleteButton = Button("Delete word", [this] { deleteSelectedWord(); }, ButtonOption::Ascii());
    Component cancel = Button("Cancel", [this] { d_wordEditorOpen = false; }, ButtonOption::Ascii());
    Component container =
        Container::Vertical({input, groups, Container::Horizontal({saveButton, deleteButton, cancel})});

    Component rendered = Renderer(container, [this, input, groups, saveButton, deleteButton, cancel] {
        const std::vector<Token>& tokens = d_form.project().tokens;
        if (d_wordSelected >= tokens.size()) {
            return text("");
        }
        const Token& token = tokens[d_wordSelected];
        return dialogWindow(" Edit word ", vbox({
            hbox({text("Word ") | bold, input->Render() | flex}),
            text("Sung at " + formatTime(token.startMs) + ", " +
                 std::to_string(token.endMs - token.startMs) + " ms") | dim,
            d_wordError.empty() ? text("") : text(d_wordError) | color(ftxui::Color::Red),
            text("Rhyme group") | bold,
            groups->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 10),
            text("Detect rhymes replaces groups set by hand") | dim,
            separator(),
            hbox({saveButton->Render(), text(" "), deleteButton->Render(), text(" "), cancel->Render(), filler(),
                  text("Enter: save  Esc: cancel") | dim}),
        })) | size(WIDTH, EQUAL, 60);
    });
    return CatchEvent(rendered, [this](const Event& event) {
        if (event == Event::Escape) {
            d_wordEditorOpen = false;
            return true;
        }
        return false;
    });
}

Component RsansApp::makeLyricsTab() {
    // Focusable, so the arrow keys can move the highlighted word
    Component view = Renderer([this](bool focused) {
        const ProjectData& project = d_form.project();
        if (project.tokens.empty()) {
            return vbox({text("No lyrics yet. Run Analyze on the Run tab.") | dim});
        }
        selectWord(d_wordSelected);

        // One row per lyric line, words colored by their rhyme group
        Elements rows;
        for (const std::vector<std::size_t>& line : lyricLines()) {
            Elements words;
            for (const std::size_t index : line) {
                const Token& token = project.tokens[index];
                Element word = text(token.text);
                if (token.rhymeIndex.has_value() && !project.colorSwatch.empty()) {
                    const std::size_t colorIndex =
                        static_cast<std::size_t>(token.rhymeIndex.value()) % project.colorSwatch.size();
                    word = word | color(toColor(project.colorSwatch[colorIndex].hex())) | bold;
                }
                if (index == d_wordSelected) {
                    // Scrolled into view; shown strongly only while the list has focus
                    word = focused ? word | inverted | focus : word | underlined | focus;
                }
                if (!words.empty()) {
                    words.push_back(text(" "));
                }
                words.push_back(word);
            }
            rows.push_back(hbox({text(formatTime(project.tokens[line.front()].startMs) + "  ") | dim,
                                 hbox(std::move(words))}));
        }

        Element note = d_rhymesOutOfDate
            ? text("Words changed: run Detect rhymes again to update the colors") | color(ftxui::Color::Yellow)
            : text(std::to_string(project.tokens.size()) + " words. Colored words rhyme.") | dim;
        const Token& selected = project.tokens[d_wordSelected];
        const std::string hint = d_runner.isRunning()
            ? "Editing waits until " + d_runner.name() + " finishes"
            : "Arrows: choose a word   Enter: edit it or its rhyme group";
        return vbox({
            note,
            text(""),
            vbox(std::move(rows)) | yframe | vscroll_indicator | flex,
            separator(),
            hbox({text("\"" + selected.text + "\" at " + formatTime(selected.startMs)) | bold, filler(),
                  text(hint) | dim}),
        });
    });

    return CatchEvent(view, [this](const Event& event) {
        const std::vector<Token>& tokens = d_form.project().tokens;
        if (tokens.empty()) {
            return false;
        }
        if (event == Event::Return) {
            openWordEditor();
            return true;
        }
        if (event == Event::ArrowRight && d_wordSelected + 1 < tokens.size()) {
            ++d_wordSelected;
            return true;
        }
        if (event == Event::ArrowLeft && d_wordSelected > 0) {
            --d_wordSelected;
            return true;
        }
        if (event == Event::ArrowUp || event == Event::ArrowDown) {
            // To the word in the line above or below at about the same place
            const std::vector<std::vector<std::size_t>> lines = lyricLines();
            for (std::size_t l = 0; l < lines.size(); ++l) {
                const auto at = std::find(lines[l].begin(), lines[l].end(), d_wordSelected);
                if (at == lines[l].end()) {
                    continue;
                }
                const bool up = event == Event::ArrowUp;
                if ((up && l == 0) || (!up && l + 1 == lines.size())) {
                    return false;  // Off the top: back to the tab bar
                }
                const std::vector<std::size_t>& target = lines[up ? l - 1 : l + 1];
                const auto position = static_cast<std::size_t>(at - lines[l].begin());
                d_wordSelected = target[std::min(position, target.size() - 1)];
                return true;
            }
        }
        return false;
    });
}

int runTui(const std::string& ffmpegPath, bool ffmpegGiven) {
    ScreenInteractive screen = ScreenInteractive::Fullscreen();
    // Ctrl+C is handled by the app (cancel and quit) instead of FTXUI's abort
    screen.ForceHandleCtrlC(false);

    // Closures don't trigger a redraw in FTXUI, so follow each with an event
    const StepRunner::Post post = [&screen](std::function<void()> task) {
        screen.Post(std::move(task));
        screen.PostEvent(Event::Custom);
    };

    {
        // Scoped so the app (and its worker thread) ends before the screen
        RsansApp app(ffmpegPath, post, screen.ExitLoopClosure());
        app.useSettingsFile(defaultSettingsPath(), ffmpegGiven);
        app.scanFonts();  // In the background, so the UI appears straight away
        screen.Loop(app.component());
    }
    return 0;
}
