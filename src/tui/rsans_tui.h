#ifndef INCLUDED_RSANS_TUI
#define INCLUDED_RSANS_TUI

#include <rsans_project_form.h>
#include <rsans_step_runner.h>

#include <ftxui/component/component_base.hpp>

#include <functional>
#include <string>
#include <vector>

// The interactive RSANS application: a Project tab to fill in settings, a
// Run tab to run the pipeline steps with progress, cancel and a log, and a
// Lyrics tab previewing the transcribed words in their rhyme colors.
class RsansApp {
  public:
    // post: runs a task on the UI thread and redraws (see StepRunner).
    // quit: ends the UI loop.
    RsansApp(std::string ffmpegPath, StepRunner::Post post, std::function<void()> quit);

    RsansApp(const RsansApp&) = delete;
    RsansApp& operator=(const RsansApp&) = delete;
    RsansApp(RsansApp&&) = delete;
    RsansApp& operator=(RsansApp&&) = delete;
    ~RsansApp() = default;

    ftxui::Component component() const { return d_root; }

    ProjectForm& form() { return d_form; }
    StepRunner& runner() { return d_runner; }

    enum class Step { Analyze, Rhyme, Export, Full };

    // Why a step can't run yet, as "field: message" lines (empty if ready)
    std::vector<std::string> problemsFor(Step step) const;

    // Starts a step if it is ready and nothing else is running
    bool runStep(Step step);

    std::string outputPath;  // Edited on the Run tab
    std::string ffmpegPath;  // App-wide setting, not saved in the project

  private:
    ProjectForm d_form;
    std::function<void()> d_quit;
    std::vector<std::string> d_tabNames;
    int d_tab = 0;
    ftxui::Component d_root;
    // Last, so it is destroyed first: it waits for the worker, which uses the
    // members above
    StepRunner d_runner;

    ftxui::Component makeProjectTab();
    ftxui::Component makeRunTab();
    ftxui::Component makeLyricsTab() const;
};

// Runs the TUI until the user quits. Returns the process exit code.
int runTui(const std::string& ffmpegPath);

#endif
