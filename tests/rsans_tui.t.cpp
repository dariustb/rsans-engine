#include <rsans_project_form.h>
#include <rsans_step_runner.h>
#include <rsans_tui.h>

#include <rsans_test_utils.h>

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

namespace {

// Stands in for the UI thread: the worker queues tasks, the test runs them
class ManualPost {
  private:
    std::mutex d_mutex;
    std::deque<std::function<void()>> d_tasks;

  public:
    StepRunner::Post post() {
        return [this](std::function<void()> task) {
            const std::lock_guard<std::mutex> lock(d_mutex);
            d_tasks.push_back(std::move(task));
        };
    }

    void runPending() {
        std::deque<std::function<void()>> tasks;
        {
            const std::lock_guard<std::mutex> lock(d_mutex);
            tasks.swap(d_tasks);
        }
        for (auto& task : tasks) {
            task();
        }
    }

    // Runs posted tasks until the runner leaves Running (or times out)
    bool waitUntilFinished(const StepRunner& runner) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            runPending();
            if (!runner.isRunning()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }
};

FormField& fieldFor(ProjectForm& form, const std::string& key) {
    for (FormField& field : form.fields()) {
        if (field.key == key) {
            return field;
        }
    }
    throw std::logic_error("No field " + key);
}

// Renders off-screen and returns the text, without terminal color codes
std::string renderToString(const ftxui::Component& component, int width = 120, int height = 60) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, component->Render());
    static const std::regex ansiCodes("\x1b\\[[0-9;]*m");
    return std::regex_replace(screen.ToString(), ansiCodes, "");
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

// ProjectForm

TEST(ProjectFormTest, FieldsShowDefaultProjectValues) {
    ProjectForm form(defaultProject());

    EXPECT_EQ(fieldFor(form, "video.width").text, "1920");
    EXPECT_EQ(fieldFor(form, "video.background").text, "black");
    EXPECT_EQ(fieldFor(form, "header.title").text, "");
    EXPECT_EQ(fieldFor(form, "colorSwatch").text.rfind("#FF8800, ", 0), 0u);
}

TEST(ProjectFormTest, ApplyWritesEditedTextToProject) {
    // Given
    ProjectForm form(defaultProject());
    fieldFor(form, "header.title").text = "My Song";
    fieldFor(form, "video.width").text = "1280";

    // When
    form.apply();

    // Then
    EXPECT_EQ(form.project().header.title, "My Song");
    EXPECT_EQ(form.project().video.width, 1280);
    EXPECT_TRUE(form.errorsFor("header.title").empty());
}

TEST(ProjectFormTest, ErrorsForShowsValidationErrorGivenBlankRequiredField) {
    ProjectForm form(defaultProject());
    EXPECT_EQ(form.errorsFor("header.title"), std::vector<std::string>{"is required"});
}

TEST(ProjectFormTest, ErrorsForShowsOnlyParseErrorGivenNonNumber) {
    // Given
    ProjectForm form(defaultProject());
    fieldFor(form, "video.width").text = "12a";

    // When
    form.apply();

    // Then: not also "must be greater than 0"
    EXPECT_EQ(form.errorsFor("video.width"), std::vector<std::string>{"must be a whole number"});
}

TEST(ProjectFormTest, ApplyKeepsValidColorsAndReportsInvalidOne) {
    // Given
    ProjectForm form(defaultProject());
    fieldFor(form, "colorSwatch").text = "#FF0000, nope #00FF00";

    // When
    form.apply();

    // Then
    ASSERT_EQ(form.project().colorSwatch.size(), 2u);
    EXPECT_EQ(form.project().colorSwatch[1].hex(), "#00FF00");
    ASSERT_EQ(form.errorsFor("colorSwatch").size(), 1u);
    EXPECT_TRUE(contains(form.errorsFor("colorSwatch")[0], "nope"));
}

TEST(ProjectFormTest, ApplyMeasuresAudioLengthGivenNewAudioPath) {
    // Given
    ProjectForm form(defaultProject());
    const std::string audio = writeTestWav("rsans_form_audio.wav", 1, 16000, 1500);
    fieldFor(form, "audio.path").text = audio;

    // When
    form.apply();

    // Then
    EXPECT_NEAR(form.project().audio.length, 1.5, 0.001);
    EXPECT_TRUE(form.errorsFor("audio.path").empty());
    std::filesystem::remove(audio);
}

TEST(ProjectFormTest, SetTokensReplacesTokens) {
    ProjectForm form(defaultProject());
    form.setTokens({Token(0, "hello", 0, 500, 0, std::nullopt)});
    ASSERT_EQ(form.project().tokens.size(), 1u);
    EXPECT_EQ(form.project().tokens[0].text, "hello");
}

// StepRunner

TEST(StepRunnerTest, StartRunsJobAndDeliversResultOnUiThread) {
    // Given
    ManualPost ui;
    StepRunner runner(ui.post());
    std::string received;

    // When
    ASSERT_TRUE(runner.start("Test", [](const StepContext& context) {
        context.onStage("Working");
        context.onLog("a log line");
        context.onProgress(0.5);
        ProjectData result;
        result.header.title = "result";
        return result;
    }, [&](ProjectData result) { received = result.header.title; }));

    // Then
    ASSERT_TRUE(ui.waitUntilFinished(runner));
    EXPECT_EQ(runner.state(), StepRunner::State::Succeeded);
    EXPECT_EQ(received, "result");
    EXPECT_EQ(runner.stage(), "Working");
    EXPECT_DOUBLE_EQ(runner.progress(), 1.0);
    EXPECT_NE(std::find(runner.log().begin(), runner.log().end(), "a log line"), runner.log().end());
}

TEST(StepRunnerTest, FailedJobReportsError) {
    ManualPost ui;
    StepRunner runner(ui.post());

    runner.start("Test", [](const StepContext&) -> ProjectData {
        throw std::runtime_error("model not found");
    }, nullptr);

    ASSERT_TRUE(ui.waitUntilFinished(runner));
    EXPECT_EQ(runner.state(), StepRunner::State::Failed);
    EXPECT_EQ(runner.error(), "model not found");
}

TEST(StepRunnerTest, CancelStopsJobAsCancelled) {
    // Given: a job that runs until cancelled
    ManualPost ui;
    StepRunner runner(ui.post());
    bool succeeded = false;
    runner.start("Test", [](const StepContext& context) -> ProjectData {
        while (!context.cancel->isRequested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        throw CancelledError("Test cancelled");
    }, [&](ProjectData) { succeeded = true; });

    // When
    runner.cancel();

    // Then
    ASSERT_TRUE(ui.waitUntilFinished(runner));
    EXPECT_EQ(runner.state(), StepRunner::State::Cancelled);
    EXPECT_FALSE(succeeded);
}

TEST(StepRunnerTest, StartReturnsFalseGivenStepAlreadyRunning) {
    // Given
    ManualPost ui;
    StepRunner runner(ui.post());
    std::atomic<bool> release{false};
    runner.start("First", [&](const StepContext&) {
        while (!release) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return ProjectData{};
    }, nullptr);

    // When / Then
    EXPECT_FALSE(runner.start("Second", [](const StepContext&) { return ProjectData{}; }, nullptr));
    release = true;
    ASSERT_TRUE(ui.waitUntilFinished(runner));
    EXPECT_EQ(runner.name(), "First");
}

TEST(StepRunnerTest, DestructorCancelsRunningJob) {
    // Given: a job that only stops when cancelled
    ManualPost ui;
    std::atomic<bool> sawCancel{false};
    {
        StepRunner runner(ui.post());
        runner.start("Test", [&](const StepContext& context) -> ProjectData {
            while (!context.cancel->isRequested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            sawCancel = true;
            throw CancelledError("Cancelled");
        }, nullptr);
    }  // When: destroyed while running (this would hang if it didn't cancel)

    // Then
    EXPECT_TRUE(sawCancel);
}

TEST(StepRunnerTest, LogKeepsOnlyMostRecentLines) {
    ManualPost ui;
    StepRunner runner(ui.post());
    runner.start("Test", [](const StepContext& context) {
        for (std::size_t i = 0; i < StepRunner::maxLogLines + 50; ++i) {
            context.onLog("line " + std::to_string(i));
        }
        return ProjectData{};
    }, nullptr);

    ASSERT_TRUE(ui.waitUntilFinished(runner));
    EXPECT_EQ(runner.log().size(), StepRunner::maxLogLines);
    EXPECT_EQ(runner.log().back(), "Test finished");
}

// RsansApp

TEST(RsansAppTest, ProjectTabShowsFieldsAndErrors) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});

    const std::string screen = renderToString(app.component());

    EXPECT_TRUE(contains(screen, "Title"));
    EXPECT_TRUE(contains(screen, "Whisper model"));
    // Blank required fields are marked "!", explained once at the bottom
    EXPECT_TRUE(contains(screen, "! = value is required"));
    const auto first = screen.find("is required");
    EXPECT_EQ(screen.find("is required", first + 1), std::string::npos) << screen;
}

TEST(RsansAppTest, ProjectTabSpellsOutErrorsOtherThanRequired) {
    // Given
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    fieldFor(app.form(), "video.width").text = "12a";
    fieldFor(app.form(), "audio.path").text = "/nonexistent/song.wav";
    app.form().apply();

    // When
    const std::string screen = renderToString(app.component());

    // Then
    EXPECT_TRUE(contains(screen, "must be a whole number")) << screen;
    EXPECT_TRUE(contains(screen, "file not found: /nonexistent/song.wav")) << screen;
}

TEST(RsansAppTest, RunTabShowsStepsAndWhyTheyCantRun) {
    // Given
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});

    // When: the tab bar has focus at start; move right to "Run"
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    const std::string screen = renderToString(app.component());

    // Then
    EXPECT_TRUE(contains(screen, "Analyze"));
    EXPECT_TRUE(contains(screen, "Full run"));
    EXPECT_TRUE(contains(screen, "Checks"));
    EXPECT_TRUE(contains(screen, "audio.path: is required"));
}

TEST(RsansAppTest, ProblemsForExportIncludeOutputPath) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});

    const std::vector<std::string> problems = app.problemsFor(RsansApp::Step::Export);
    EXPECT_NE(std::find(problems.begin(), problems.end(), "output video: is required"), problems.end());

    app.outputPath = "out.ass";
    const std::vector<std::string> assProblems = app.problemsFor(RsansApp::Step::Export);
    EXPECT_NE(std::find(assProblems.begin(), assProblems.end(), "output video: must not end in .ass"),
              assProblems.end());
}

TEST(RsansAppTest, RunStepDoesNothingGivenStepNotReady) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});

    EXPECT_FALSE(app.runStep(RsansApp::Step::Analyze));
    EXPECT_EQ(app.runner().state(), StepRunner::State::Idle);
}

TEST(RsansAppTest, RunStepDetectsRhymesAndShowsThemInLyricsTab) {
    // Given: a small dictionary and transcribed words
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    const std::filesystem::path dict = std::filesystem::temp_directory_path() / "rsans_tui_test.dict";
    std::ofstream(dict) << "cat K AE1 T\nhat HH AE1 T\ndog D AO1 G\n";
    fieldFor(app.form(), "cmudict").text = dict.string();
    app.form().apply();
    app.form().setTokens({Token(0, "cat", 0, 400, 0, std::nullopt),
                          Token(1, "hat", 400, 800, 0, std::nullopt),
                          Token(2, "dog", 800, 1200, 1, std::nullopt)});

    // When
    ASSERT_TRUE(app.runStep(RsansApp::Step::Rhyme));
    ASSERT_TRUE(ui.waitUntilFinished(app.runner()));

    // Then: the result reached the form
    EXPECT_EQ(app.runner().state(), StepRunner::State::Succeeded);
    const std::vector<Token>& tokens = app.form().project().tokens;
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_TRUE(tokens[0].rhymeIndex.has_value());
    EXPECT_EQ(tokens[0].rhymeIndex, tokens[1].rhymeIndex);
    EXPECT_FALSE(tokens[2].rhymeIndex.has_value());

    // And the Lyrics tab shows the words, one row per line
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    const std::string screen = renderToString(app.component());
    EXPECT_TRUE(contains(screen, "cat hat")) << screen;
    EXPECT_TRUE(contains(screen, "3 words"));
    EXPECT_TRUE(contains(screen, "dog"));
    std::filesystem::remove(dict);
}

TEST(RsansAppTest, RunStepReportsFailureGivenMissingModel) {
    // Given: inputs analyze checks are present, but the model file is empty junk
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    const std::string audio = writeTestWav("rsans_tui_analyze.wav", 1, 16000);
    const std::filesystem::path model = std::filesystem::temp_directory_path() / "rsans_tui_model.bin";
    std::ofstream(model) << "not a model";
    fieldFor(app.form(), "audio.path").text = audio;
    fieldFor(app.form(), "model.path").text = model.string();
    app.form().apply();
    ASSERT_TRUE(app.problemsFor(RsansApp::Step::Analyze).empty());

    // When
    ASSERT_TRUE(app.runStep(RsansApp::Step::Analyze));
    ASSERT_TRUE(ui.waitUntilFinished(app.runner()));

    // Then
    EXPECT_EQ(app.runner().state(), StepRunner::State::Failed);
    EXPECT_TRUE(contains(app.runner().error(), "Failed to load Whisper model"));
    std::filesystem::remove(audio);
    std::filesystem::remove(model);
}

TEST(RsansAppTest, CtrlCQuitsApp) {
    ManualPost ui;
    bool quit = false;
    RsansApp app("ffmpeg", ui.post(), [&] { quit = true; });

    EXPECT_TRUE(app.component()->OnEvent(ftxui::Event::CtrlC));
    EXPECT_TRUE(quit);
}

TEST(RsansAppTest, TypingIntoFieldUpdatesProjectAndErrors) {
    // Given: focus starts on the tab bar; down moves into the first field (Title)
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    ASSERT_FALSE(app.form().errorsFor("header.title").empty());
    app.component()->OnEvent(ftxui::Event::ArrowDown);

    // When
    for (const char c : std::string("Hi")) {
        app.component()->OnEvent(ftxui::Event::Character(c));
    }

    // Then
    EXPECT_EQ(app.form().project().header.title, "Hi");
    EXPECT_TRUE(app.form().errorsFor("header.title").empty());
}

// Font picker

namespace {

std::vector<FontInfo> pickerFonts() {
    return {
        {"DejaVu Sans", "Book", "/fonts/DejaVuSans.ttf"},
        {"DejaVu Sans Mono", "Book", "/fonts/DejaVuSansMono.ttf"},
        {"Noto Serif", "Regular", "/fonts/NotoSerif.ttf"},
    };
}

void type(RsansApp& app, const std::string& text) {
    for (const char c : text) {
        app.component()->OnEvent(ftxui::Event::Character(c));
    }
}

} // namespace

TEST(FontPickerTest, TypingAndEnterChoosesMatchingFont) {
    // Given
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.setFonts(pickerFonts());
    app.openFontPicker(RsansApp::FontTarget::Header);

    // When: the search box has focus in the open picker
    type(app, "mono");
    app.component()->OnEvent(ftxui::Event::Return);

    // Then: both the name and the file are filled in
    EXPECT_FALSE(app.fontPickerOpen());
    EXPECT_EQ(app.form().project().header.fontName, "DejaVu Sans Mono");
    EXPECT_EQ(app.form().project().header.fontPath, "/fonts/DejaVuSansMono.ttf");
    EXPECT_EQ(app.form().project().layout.fontName, "");
}

TEST(FontPickerTest, ArrowDownAndEnterChoosesHighlightedFont) {
    // Given
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.setFonts(pickerFonts());
    app.openFontPicker(RsansApp::FontTarget::Layout);
    type(app, "dejavu");

    // When: down into the list, then down to the second match
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_EQ(app.form().project().layout.fontName, "DejaVu Sans Mono");
}

TEST(FontPickerTest, EscapeClosesWithoutChanging) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.setFonts(pickerFonts());
    app.openFontPicker(RsansApp::FontTarget::Header);
    type(app, "noto");

    app.component()->OnEvent(ftxui::Event::Escape);

    EXPECT_FALSE(app.fontPickerOpen());
    EXPECT_EQ(app.form().project().header.fontName, "");
}

TEST(FontPickerTest, PickerShowsMatchCountAndHighlightedFile) {
    // Given
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.setFonts(pickerFonts());
    app.openFontPicker(RsansApp::FontTarget::Header);
    type(app, "dejavu");

    // When
    const std::string screen = renderToString(app.component());

    // Then
    EXPECT_TRUE(contains(screen, "Header font")) << screen;
    EXPECT_TRUE(contains(screen, "2 of 3 fonts")) << screen;
    EXPECT_TRUE(contains(screen, "/fonts/DejaVuSans.ttf")) << screen;
}

TEST(FontPickerTest, ProjectTabShowsChosenFontNameAndFile) {
    // Given
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.setFonts(pickerFonts());
    EXPECT_TRUE(contains(renderToString(app.component()), "Choose a font..."));

    // When
    app.openFontPicker(RsansApp::FontTarget::Header);
    type(app, "noto");
    app.component()->OnEvent(ftxui::Event::Return);
    const std::string screen = renderToString(app.component());

    // Then
    EXPECT_TRUE(contains(screen, "Noto Serif")) << screen;
    EXPECT_TRUE(contains(screen, "/fonts/NotoSerif.ttf")) << screen;
}

TEST(FontPickerTest, TypedFontFileCanBeChosenDirectly) {
    const std::string dejaVu = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    if (!std::filesystem::exists(dejaVu)) {
        GTEST_SKIP() << "DejaVu Sans not installed";
    }
    // Given: no installed fonts listed at all
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.openFontPicker(RsansApp::FontTarget::Layout);

    // When
    type(app, dejaVu);
    app.component()->OnEvent(ftxui::Event::Return);

    // Then: the family is read from the file
    EXPECT_EQ(app.form().project().layout.fontName, "DejaVu Sans");
    EXPECT_EQ(app.form().project().layout.fontPath, dejaVu);
    EXPECT_TRUE(app.form().errorsFor("layout.fontPath").empty());
}

TEST(FontPickerTest, ScanFontsLoadsSystemFontsInBackground) {
    // Given
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});

    // When
    app.scanFonts();
    EXPECT_TRUE(app.fontsScanning());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (app.fontsScanning() && std::chrono::steady_clock::now() < deadline) {
        ui.runPending();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Then
    EXPECT_FALSE(app.fontsScanning());
    if (app.fonts().empty()) {
        GTEST_SKIP() << "No fonts installed";
    }
    EXPECT_FALSE(app.fonts().front().family.empty());
}

TEST(FontPickerTest, EnterOnFontButtonOpensPicker) {
    // Given: focus starts on the tab bar; Header font is the 5th row of the Project tab
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.setFonts(pickerFonts());
    for (int i = 0; i < 5; ++i) {
        app.component()->OnEvent(ftxui::Event::ArrowDown);
    }

    // When
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_TRUE(app.fontPickerOpen()) << renderToString(app.component());
}


TEST(RsansAppTest, OneDownPressMovesOneRowGivenFieldsWithValues) {
    // Given: Title size, Width and Height already have values, which FTXUI's
    // input would otherwise use the first Down press on (cursor to the end)
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});

    // When: tab bar -> Title -> Title size -> Artist
    for (int i = 0; i < 3; ++i) {
        app.component()->OnEvent(ftxui::Event::ArrowDown);
    }
    app.component()->OnEvent(ftxui::Event::Character('X'));

    // Then
    EXPECT_EQ(app.form().project().header.artist, "X");
}

// File dialog

namespace {

// A temporary folder holding cover.png, notes.txt and sub/song.wav
struct DialogFolder {
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("rsans_dialog_" + std::to_string(::getpid()));

    DialogFolder() {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "sub");
        writeTestBmp((root / "cover.png").string());
        std::ofstream(root / "notes.txt") << "x";
        std::ofstream(root / "sub" / "song.wav") << "x";
    }
    ~DialogFolder() { std::filesystem::remove_all(root); }
    DialogFolder(const DialogFolder&) = delete;
    DialogFolder& operator=(const DialogFolder&) = delete;
    DialogFolder(DialogFolder&&) = delete;
    DialogFolder& operator=(DialogFolder&&) = delete;
};

} // namespace

TEST(FileDialogTest, EnterChoosesFirstMatchingFileForField) {
    // Given: the cover field points into the folder, so the dialog starts there
    DialogFolder folder;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.form().setFieldText("header.media", (folder.root / "missing.png").string());
    app.browseFor("header.media");
    ASSERT_TRUE(app.fileDialogOpen());
    EXPECT_EQ(app.fileBrowser()->directory(), folder.root);

    // When: images only, so notes.txt is hidden; type part of the name and Enter
    type(app, "cov");
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_EQ(app.form().project().header.media, (folder.root / "cover.png").string());
    EXPECT_TRUE(app.form().errorsFor("header.media").empty());
}

TEST(FileDialogTest, ListNavigationOpensFolderThenChoosesFile) {
    DialogFolder folder;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.form().setFieldText("audio.path", folder.root.string());
    app.browseFor("audio.path");

    // Down into the list (sub/ is highlighted, below ..), Enter opens it
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::Return);
    EXPECT_EQ(app.fileBrowser()->directory(), folder.root / "sub");
    EXPECT_TRUE(app.fileDialogOpen());

    // song.wav is now highlighted
    app.component()->OnEvent(ftxui::Event::Return);

    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_EQ(app.form().project().audio.path, (folder.root / "sub" / "song.wav").string());
}

TEST(FileDialogTest, BackspaceInEmptySearchGoesUpAndEscapeCloses) {
    DialogFolder folder;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.form().setFieldText("audio.path", (folder.root / "sub").string());
    app.browseFor("audio.path");

    app.component()->OnEvent(ftxui::Event::Backspace);
    EXPECT_EQ(app.fileBrowser()->directory(), folder.root);

    app.component()->OnEvent(ftxui::Event::Escape);
    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_EQ(app.form().project().audio.path, (folder.root / "sub").string());
}

TEST(FileDialogTest, SavingOutputAddsMp4ToTypedName) {
    DialogFolder folder;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.outputPath = (folder.root / "old.mp4").string();
    app.browseFor("output");
    EXPECT_EQ(app.fileQuery, "old.mp4");

    // When: replace the name and press Enter
    for (int i = 0; i < 7; ++i) {
        app.component()->OnEvent(ftxui::Event::Backspace);
    }
    type(app, "clip");
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_EQ(app.outputPath, (folder.root / "clip.mp4").string());
}

TEST(FileDialogTest, SavingOutputRejectsAssName) {
    DialogFolder folder;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.outputPath = (folder.root / "lyrics.ass").string();
    app.browseFor("output");

    app.component()->OnEvent(ftxui::Event::Return);

    EXPECT_TRUE(app.fileDialogOpen());
    EXPECT_EQ(app.outputPath, (folder.root / "lyrics.ass").string());
    EXPECT_TRUE(contains(renderToString(app.component()), "can't be an .ass file"));
}

TEST(FileDialogTest, FontPickerBrowseRejectsNonFontAndAcceptsFont) {
    DialogFolder folder;
    std::ofstream(folder.root / "fake.ttf") << "not a font";
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.form().setFieldText("layout.fontPath", (folder.root / "fake.ttf").string());
    app.openFontPicker(RsansApp::FontTarget::Layout);

    // When: the picker's Browse files button (search -> list -> buttons row -> right)
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    app.component()->OnEvent(ftxui::Event::Return);
    ASSERT_TRUE(app.fileDialogOpen()) << renderToString(app.component());
    type(app, "fake");
    app.component()->OnEvent(ftxui::Event::Return);

    // Then: not a font, so it stays open with the reason
    EXPECT_TRUE(app.fileDialogOpen());
    EXPECT_TRUE(contains(renderToString(app.component()), "Not a font file")) << renderToString(app.component());

    // When: a real font is typed instead
    const std::string dejaVu = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    if (!std::filesystem::exists(dejaVu)) {
        GTEST_SKIP() << "DejaVu Sans not installed";
    }
    for (int i = 0; i < 4; ++i) {
        app.component()->OnEvent(ftxui::Event::Backspace);  // "fake"
    }
    type(app, dejaVu);
    app.component()->OnEvent(ftxui::Event::Return);

    // Then: both dialogs close and the font is filled in from the file
    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_FALSE(app.fontPickerOpen());
    EXPECT_EQ(app.form().project().layout.fontName, "DejaVu Sans");
    EXPECT_EQ(app.form().project().layout.fontPath, dejaVu);
}

TEST(FileDialogTest, ProjectTabBrowseButtonOpensDialog) {
    // Given: Cover image is the 6th row; Right moves from its input to Browse
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    EXPECT_TRUE(contains(renderToString(app.component()), "Browse"));
    for (int i = 0; i < 6; ++i) {
        app.component()->OnEvent(ftxui::Event::ArrowDown);
    }
    app.component()->OnEvent(ftxui::Event::ArrowRight);

    // When
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    ASSERT_TRUE(app.fileDialogOpen());
    const std::string screen = renderToString(app.component());
    EXPECT_TRUE(contains(screen, "Cover image")) << screen;
    EXPECT_TRUE(contains(screen, ".png .jpg")) << screen;
}
