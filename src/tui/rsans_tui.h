#ifndef INCLUDED_RSANS_TUI
#define INCLUDED_RSANS_TUI

#include <rsans_fonts.h>
#include <rsans_project_form.h>
#include <rsans_step_runner.h>

#include <ftxui/component/component_base.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// The interactive RSANS application: a Project tab to fill in settings, a
// Run tab to run the pipeline steps with progress, cancel and a log, and a
// Lyrics tab previewing the transcribed words in their rhyme colors. Fonts
// are chosen from the system's installed fonts with a type-to-search picker.
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

    std::vector<std::string> d_tabNames;
    int d_tab = 0;
    ftxui::Component d_root;
    // Last, so it is destroyed first: it waits for the worker, which uses the
    // members above
    StepRunner d_runner;

    ftxui::Component makeProjectTab();
    ftxui::Component makeRunTab();
    ftxui::Component makeLyricsTab() const;
    ftxui::Component makeFontPicker();

    void refreshFontMatches();
    void chooseSelectedFont();
    void chooseFont(const FontInfo& font);
};

// Runs the TUI until the user quits. Returns the process exit code.
int runTui(const std::string& ffmpegPath);

#endif
