#ifndef INCLUDED_RSANS_TUI
#define INCLUDED_RSANS_TUI

#include <rsans_file_browser.h>
#include <rsans_fonts.h>
#include <rsans_project_form.h>
#include <rsans_step_runner.h>
#include <rsans_whisper_models.h>

#include <ftxui/component/component_base.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// The interactive RSANS application. It opens on a start screen (new project,
// load project, credits, quit), then an editor with a Project tab to fill in settings, a
// Run tab to run the pipeline steps with progress, cancel and a log, and a
// Lyrics tab previewing the transcribed words in their rhyme colors. Fonts
// are chosen from the system's installed fonts with a type-to-search picker,
// and every file path can be typed or picked with a file dialog.
class RsansApp {
  public:
    // post: runs a task on the UI thread and redraws (see StepRunner).
    // quit: ends the UI loop.
    RsansApp(std::string ffmpegPath, StepRunner::Post post, std::function<void()> quit);

    RsansApp(const RsansApp&) = delete;
    RsansApp& operator=(const RsansApp&) = delete;
    RsansApp(RsansApp&&) = delete;
    RsansApp& operator=(RsansApp&&) = delete;
    ~RsansApp();  // Waits for a font scan in progress

    ftxui::Component component() const { return d_root; }

    enum class Screen { Start, Credits, Editor };
    Screen screen() const { return static_cast<Screen>(d_screen); }

    // Starts a blank project (with default settings) in the editor
    void newProject();
    // Loads a project JSON file into the editor. Returns "" on success, or
    // why it couldn't be loaded (the current project is left unchanged).
    std::string loadProject(const std::string& path);

    ProjectForm& form() { return d_form; }
    StepRunner& runner() { return d_runner; }

    enum class Step { Analyze, Rhyme, Export, Full };

    // Why a step can't run yet, as "field: message" lines (empty if ready)
    std::vector<std::string> problemsFor(Step step) const;

    // Starts a step if it is ready and nothing else is running
    bool runStep(Step step);

    // Which font setting the picker fills in
    enum class FontTarget { Header, Layout };

    // Lists the system's fonts on a background thread; the list arrives via
    // `post`. Does nothing if a scan is already running.
    void scanFonts();
    bool fontsScanning() const { return d_fontsScanning; }
    const std::vector<FontInfo>& fonts() const { return d_fonts; }
    void setFonts(std::vector<FontInfo> fonts);  // Also used by tests

    void openFontPicker(FontTarget target);
    bool fontPickerOpen() const { return d_pickerOpen; }
    std::string fontQuery;  // The picker's search text

    // Whisper models: a short list of the sizes whisper.cpp offers (all of
    // them with "Show all"), marking which are installed. Choosing one that isn't asks to download it with
    // whisper.cpp's own download script (run as a step, with progress).
    std::string whisperDir;  // App-wide: the whisper.cpp checkout (see defaultWhisperDir)
    void refreshWhisperModels();
    const std::vector<WhisperModel>& whisperModels() const { return d_models; }
    void openModelPicker();
    bool modelPickerOpen() const { return d_modelPickerOpen; }
    // The model waiting for the user to confirm its download, if any
    std::optional<std::string> modelAwaitingDownload() const;
    // Starts downloading a model (by name). False if it can't start now.
    bool downloadModel(const std::string& name);

    // Called with the path chosen in the file dialog. Returns "" to accept it
    // (the dialog closes) or an error to show (the dialog stays open).
    using FileChosen = std::function<std::string(const std::string&)>;

    // Opens the file dialog. extensions: file types listed by default (see
    // FileBrowser); saveMode: for a file to be written, which may not exist
    void openFileDialog(std::string title, std::vector<std::string> extensions, bool saveMode,
                        const std::string& current, FileChosen onChoose);
    // Opens the file dialog for a project field (by key) or "output"/"ffmpeg"
    void browseFor(const std::string& key);
    bool fileDialogOpen() const { return d_fileDialogOpen; }
    const FileBrowser* fileBrowser() const { return d_browser ? &*d_browser : nullptr; }
    std::string fileQuery;  // The file dialog's search (or, saving, file name) text

    std::string outputPath;  // Edited on the Run tab
    std::string ffmpegPath;  // App-wide setting, not saved in the project

  private:
    ProjectForm d_form;
    std::function<void()> d_quit;
    StepRunner::Post d_post;

    // Fonts and the picker
    std::vector<FontInfo> d_fonts;
    bool d_fontsScanning = false;
    std::thread d_fontScan;
    bool d_pickerOpen = false;
    FontTarget d_pickerTarget = FontTarget::Header;
    std::optional<FontInfo> d_fileMatch;      // Set when the search is a font file path
    std::vector<std::size_t> d_matchIndexes;  // Into d_fonts
    std::vector<std::string> d_matchLabels;   // Shown in the picker list
    int d_matchSelected = 0;
    std::string d_headerFontLabel;
    std::string d_layoutFontLabel;

    // The Whisper model picker
    std::vector<WhisperModel> d_models;
    std::vector<std::size_t> d_modelIndexes;  // The listed models, into d_models
    std::vector<std::string> d_modelLabels;
    int d_modelSelected = 0;
    bool d_modelShowAll = false;
    bool d_modelPickerOpen = false;
    int d_modelView = 0;  // 0: the list, 1: confirm a download
    std::optional<std::size_t> d_modelToDownload;
    std::string d_modelButtonLabel;

    // The file dialog
    std::optional<FileBrowser> d_browser;
    bool d_fileDialogOpen = false;
    std::string d_fileTitle;
    FileChosen d_onFileChosen;
    std::vector<std::string> d_fileLabels;
    int d_fileSelected = 0;
    int d_fileCursor = 0;  // In the search box
    bool d_showAllFiles = false;
    std::string d_fileError;

    // The start screen
    int d_screen = 0;  // A Screen
    std::vector<std::string> d_startEntries;
    int d_startSelected = 0;

    std::vector<std::string> d_tabNames;
    int d_tab = 0;
    ftxui::Component d_root;
    // Last, so it is destroyed first: it waits for the worker, which uses the
    // members above
    StepRunner d_runner;

    ftxui::Component makeStartScreen();
    ftxui::Component makeCredits();
    void chooseStartEntry();

    ftxui::Component makeProjectTab();
    ftxui::Component makeRunTab();
    ftxui::Component makeLyricsTab() const;
    ftxui::Component makeFontPicker();
    ftxui::Component makeFileDialog();
    ftxui::Component makeModelPicker();
    void chooseSelectedModel();
    void refreshModelList();
    ftxui::Component browseButton(const std::string& key);

    void refreshFileLabels();
    void finishFileAction(const std::optional<std::string>& chosen);

    void refreshFontMatches();
    void chooseSelectedFont();
    void chooseFont(const FontInfo& font);
};

// Runs the TUI until the user quits. Returns the process exit code.
int runTui(const std::string& ffmpegPath);

#endif
