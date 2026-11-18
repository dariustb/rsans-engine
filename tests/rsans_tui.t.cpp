#include <rsans_project_form.h>
#include <rsans_step_runner.h>
#include <rsans_title.h>
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
    app.newProject();

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
    app.newProject();
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
    app.newProject();

    // When: the tab bar has focus at start; move right to "Run"
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    const std::string screen = renderToString(app.component());

    // Then
    EXPECT_TRUE(contains(screen, "Analyze"));
    EXPECT_TRUE(contains(screen, "Full run"));
    EXPECT_TRUE(contains(screen, "Checks"));
    EXPECT_TRUE(contains(screen, "audio.path: is required"));
}

TEST(RsansAppTest, ProblemsForExportDontIncludeOutputPath) {
    // Where to save the video is asked when the step starts
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();

    for (const std::string& problem : app.problemsFor(RsansApp::Step::Export)) {
        EXPECT_EQ(problem.find("output"), std::string::npos) << problem;
    }
}

TEST(RsansAppTest, RunStepDoesNothingGivenStepNotReady) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();

    EXPECT_FALSE(app.runStep(RsansApp::Step::Analyze));
    EXPECT_EQ(app.runner().state(), StepRunner::State::Idle);
}

TEST(RsansAppTest, RunStepDetectsRhymesAndShowsThemInLyricsTab) {
    // Given: a small dictionary and transcribed words
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
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
    app.newProject();
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
    app.newProject();

    EXPECT_TRUE(app.component()->OnEvent(ftxui::Event::CtrlC));
    EXPECT_TRUE(quit);
}

TEST(RsansAppTest, TypingIntoFieldUpdatesProjectAndErrors) {
    // Given: focus starts on the tab bar; down moves into the first field (Title)
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
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
    app.newProject();
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
    app.newProject();
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
    app.newProject();
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
    app.newProject();
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
    app.newProject();
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
    app.newProject();
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
    app.newProject();

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
    app.newProject();
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
    app.newProject();

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
    app.newProject();
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
    app.newProject();
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
    app.newProject();
    app.form().setFieldText("audio.path", (folder.root / "sub").string());
    app.browseFor("audio.path");

    app.component()->OnEvent(ftxui::Event::Backspace);
    EXPECT_EQ(app.fileBrowser()->directory(), folder.root);

    app.component()->OnEvent(ftxui::Event::Escape);
    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_EQ(app.form().project().audio.path, (folder.root / "sub").string());
}

TEST(FileDialogTest, FontPickerBrowseRejectsNonFontAndAcceptsFont) {
    DialogFolder folder;
    std::ofstream(folder.root / "fake.ttf") << "not a font";
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
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
    app.newProject();
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

// Start screen

TEST(StartScreenTest, AppOpensOnStartScreenWithOptions) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});

    const std::string screen = renderToString(app.component());

    EXPECT_EQ(app.screen(), RsansApp::Screen::Start);
    for (const char* option : {"New project", "Load project", "Credits", "Quit"}) {
        EXPECT_TRUE(contains(screen, option)) << option << "\n" << screen;
    }
}

TEST(StartScreenTest, NewProjectOpensEditorWithDefaults) {
    // Given: a project was being edited
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.form().setFieldText("header.title", "Old Song");

    // When: New project, from the start screen
    RsansApp fresh("ffmpeg", ui.post(), [] {});
    fresh.component()->OnEvent(ftxui::Event::Return);
    app.newProject();

    // Then
    EXPECT_EQ(fresh.screen(), RsansApp::Screen::Editor);
    EXPECT_TRUE(contains(renderToString(fresh.component()), "Title size"));
    EXPECT_EQ(app.form().project().header.title, "");
    EXPECT_EQ(fieldFor(app.form(), "header.title").text, "");
    EXPECT_EQ(app.form().project().video.width, 1920);
}

TEST(StartScreenTest, CreditsListsLibrariesAndEscapeGoesBack) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});

    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::Return);

    ASSERT_EQ(app.screen(), RsansApp::Screen::Credits);
    const std::string screen = renderToString(app.component());
    const std::array<std::string, 12> names = {
        "FFmpeg", "Whisper.cpp", "CMU Pronouncing Dictionary",
        "FTXUI", "CLI11", "nlohmann/json", "libass",
        "libsndfile", "dr_libs", "stb"
    };
    for (const std::string name : names) {
        EXPECT_TRUE(contains(screen, name)) << name << "\n" << screen;
    }
    // After the libraries
    EXPECT_GT(screen.find("by Darius Brown ヽ(^o^)ノ"), screen.find("stb")) << screen;

    app.component()->OnEvent(ftxui::Event::Escape);
    EXPECT_EQ(app.screen(), RsansApp::Screen::Start);

    // Enter on Back also returns
    app.component()->OnEvent(ftxui::Event::Return);
    app.component()->OnEvent(ftxui::Event::Return);
    EXPECT_EQ(app.screen(), RsansApp::Screen::Start);
}

TEST(StartScreenTest, QuitCallsQuit) {
    ManualPost ui;
    bool quit = false;
    RsansApp app("ffmpeg", ui.post(), [&] { quit = true; });

    for (int i = 0; i < 3; ++i) {
        app.component()->OnEvent(ftxui::Event::ArrowDown);
    }
    app.component()->OnEvent(ftxui::Event::Return);

    EXPECT_TRUE(quit);
}

namespace {

// A saved project: title, artist, sizes and one transcribed word
std::filesystem::path writeTestProject(const std::filesystem::path& folder) {
    ProjectData project = defaultProject();
    project.header.title = "Loaded Song";
    project.header.artist = "Some Artist";
    project.video.width = 1280;
    project.audio.length = 12.5;
    project.tokens.emplace_back(0, "hello", 0, 500, 0, std::nullopt);
    const std::filesystem::path path = folder / "song.rsans";
    std::ofstream(path) << project.toJson();
    return path;
}

} // namespace

TEST(StartScreenTest, LoadProjectChoosesJsonFileAndOpensEditor) {
    // Given: Load project opens the file dialog, listing project files
    DialogFolder folder;
    const std::filesystem::path path = writeTestProject(folder.root);
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::Return);
    ASSERT_TRUE(app.fileDialogOpen());
    EXPECT_TRUE(contains(renderToString(app.component()), "Load project"));

    // When
    type(app, path.string());
    app.component()->OnEvent(ftxui::Event::Return);

    // Then: every field shows the loaded values, and the words are kept
    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_EQ(app.screen(), RsansApp::Screen::Editor);
    EXPECT_EQ(fieldFor(app.form(), "header.title").text, "Loaded Song");
    EXPECT_EQ(fieldFor(app.form(), "video.width").text, "1280");
    EXPECT_EQ(app.form().project().header.artist, "Some Artist");
    EXPECT_DOUBLE_EQ(app.form().project().audio.length, 12.5);
    ASSERT_EQ(app.form().project().tokens.size(), 1u);
    EXPECT_EQ(app.form().project().tokens[0].text, "hello");
}

TEST(StartScreenTest, LoadProjectReportsBadFileAndStaysOpen) {
    DialogFolder folder;
    std::ofstream(folder.root / "broken.json") << "{ not json";
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});

    EXPECT_NE(app.loadProject((folder.root / "broken.json").string()), "");
    EXPECT_NE(app.loadProject((folder.root / "missing.json").string()), "");
    EXPECT_EQ(app.screen(), RsansApp::Screen::Start);

    // Through the dialog, the reason is shown and it stays open
    app.openFileDialog("Load project", {".json"}, false, folder.root.string(),
                       [&app](const std::string& p) { return app.loadProject(p); });
    type(app, "broken");
    app.component()->OnEvent(ftxui::Event::Return);
    EXPECT_TRUE(app.fileDialogOpen());
    EXPECT_TRUE(contains(renderToString(app.component()), "Can't load project")) << renderToString(app.component());
}

// Title art

TEST(TitleArtTest, AnsiLineAppliesColorsAndResets) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(6), ftxui::Dimension::Fixed(1));
    ftxui::Render(screen, ansiLine("a\033[0;94;47mb\033[0;34mc\033[0md\033[97;100me\033[39;49mf"));

    EXPECT_EQ(screen.PixelAt(0, 0).character, "a");
    EXPECT_EQ(screen.PixelAt(0, 0).foreground_color, ftxui::Color());
    EXPECT_EQ(screen.PixelAt(1, 0).foreground_color, ftxui::Color(ftxui::Color::BlueLight));
    EXPECT_EQ(screen.PixelAt(1, 0).background_color, ftxui::Color(ftxui::Color::GrayLight));
    EXPECT_EQ(screen.PixelAt(2, 0).foreground_color, ftxui::Color(ftxui::Color::Blue));
    EXPECT_EQ(screen.PixelAt(2, 0).background_color, ftxui::Color());  // 0 reset the background
    EXPECT_EQ(screen.PixelAt(3, 0).foreground_color, ftxui::Color());
    EXPECT_EQ(screen.PixelAt(4, 0).foreground_color, ftxui::Color(ftxui::Color::White));
    EXPECT_EQ(screen.PixelAt(4, 0).background_color, ftxui::Color(ftxui::Color::GrayDark));
    EXPECT_EQ(screen.PixelAt(5, 0).foreground_color, ftxui::Color());
    EXPECT_EQ(screen.PixelAt(5, 0).background_color, ftxui::Color());
}

TEST(TitleArtTest, TitleArtIsFullSizeWithColoredBlocks) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(titleArtWidth),
                                        ftxui::Dimension::Fixed(titleArtHeight));
    ftxui::Render(screen, titleArt());

    // First row of the R, and the colors title_text.py gives it
    EXPECT_EQ(screen.PixelAt(0, 0).character, "▄");
    EXPECT_EQ(screen.PixelAt(0, 0).foreground_color, ftxui::Color(ftxui::Color::White));
    EXPECT_EQ(screen.PixelAt(2, 0).foreground_color, ftxui::Color(ftxui::Color::BlueLight));
    EXPECT_EQ(screen.PixelAt(0, 1).background_color, ftxui::Color(ftxui::Color::GrayLight));
    // Every row fills the full width, the last ends with spaces
    EXPECT_EQ(screen.PixelAt(titleArtWidth - 1, 0).character, "▄");
    EXPECT_EQ(screen.PixelAt(0, titleArtHeight - 1).character, "▀");
}

// Whisper model picker

namespace {

// A stand-in whisper.cpp checkout. Its download script lists tiny, base.en,
// medium and medium-q5_0, prints curl-style progress, and writes the model;
// models/mode set to "fail" makes it fail, "slow" makes it hang (in a child).
struct FakeWhisperCpp {
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("rsans_fake_whisper_" + std::to_string(::getpid()));
    std::filesystem::path models = root / "models";

    FakeWhisperCpp() {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(models);
        std::ofstream(models / "download-ggml-model.sh") << R"(#!/bin/sh
models="tiny
base.en
medium
medium-q5_0"
mode=$(cat "$(dirname "$0")/mode" 2>/dev/null)
echo "Downloading ggml model $1 from 'https://huggingface.co/ggerganov/whisper.cpp' ..."
printf '  %% Total    %% Received %% Xferd  Average Speed\r\n'
printf ' 50  142M   50 71.0M    0     0  10.0M      0\r'
case "$mode" in
    fail) printf partial > "$2/ggml-$1.bin"; echo "Failed to download ggml model $1"; exit 1 ;;
    slow) printf partial > "$2/ggml-$1.bin"; sleep 30 ;;
esac
printf ' 100  142M  100  142M    0     0  10.0M      0\r'
printf 'model' > "$2/ggml-$1.bin"
echo "Done! Model '$1' saved in '$2/ggml-$1.bin'"
)";
        std::ofstream(models / "README.md") << R"(
| Model  | Disk    | SHA   |
| tiny   | 75 MiB  | `bd5` |
| base.en| 142 MiB | `137` |
| medium | 1.5 GiB | `fd9` |
)";
        std::ofstream(models / "ggml-tiny.bin") << "model";
    }
    ~FakeWhisperCpp() { std::filesystem::remove_all(root); }
    FakeWhisperCpp(const FakeWhisperCpp&) = delete;
    FakeWhisperCpp& operator=(const FakeWhisperCpp&) = delete;
    FakeWhisperCpp(FakeWhisperCpp&&) = delete;
    FakeWhisperCpp& operator=(FakeWhisperCpp&&) = delete;

    void setMode(const std::string& mode) const { std::ofstream(models / "mode") << mode; }
};

} // namespace

TEST(ModelPickerTest, PickerListsWhisperModelsWithSizesForUninstalled) {
    FakeWhisperCpp whisper;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.whisperDir = whisper.root.string();

    app.openModelPicker();
    const std::string screen = renderToString(app.component());

    // Then: the main models, names in a column, and no quantized variant
    EXPECT_TRUE(app.modelPickerOpen());
    EXPECT_TRUE(contains(screen, "tiny     ✓")) << screen;
    EXPECT_TRUE(contains(screen, "base.en  (142 MiB)")) << screen;
    EXPECT_TRUE(contains(screen, "medium   (1.5 GiB)")) << screen;
    EXPECT_FALSE(contains(screen, "medium-q5_0")) << screen;
}

TEST(ModelPickerTest, ShowAllListsEveryModel) {
    FakeWhisperCpp whisper;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.whisperDir = whisper.root.string();
    app.openModelPicker();

    // When: past the last model to the Show all checkbox, and tick it
    for (int i = 0; i < 3; ++i) {
        app.component()->OnEvent(ftxui::Event::ArrowDown);
    }
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    const std::string screen = renderToString(app.component());
    EXPECT_TRUE(contains(screen, "medium-q5_0  (~")) << screen;
}

TEST(ModelPickerTest, ChoosingInstalledModelSetsPath) {
    FakeWhisperCpp whisper;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.whisperDir = whisper.root.string();
    EXPECT_TRUE(contains(renderToString(app.component()), "Choose a model..."));

    // When: tiny is highlighted first
    app.openModelPicker();
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_FALSE(app.modelPickerOpen());
    EXPECT_EQ(app.form().project().model.base, (whisper.models / "ggml-tiny.bin").string());
    EXPECT_TRUE(app.form().errorsFor("model.path").empty());
    const std::string screen = renderToString(app.component());
    EXPECT_TRUE(contains(screen, "[tiny]") || contains(screen, " tiny ")) << screen;
}

TEST(ModelPickerTest, UninstalledModelAsksThenDownloadsWithProgress) {
    // Given
    FakeWhisperCpp whisper;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.whisperDir = whisper.root.string();
    app.openModelPicker();

    // When: base.en is chosen, a confirmation is shown first
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::Return);
    ASSERT_EQ(app.modelAwaitingDownload(), "base.en");
    const std::string prompt = renderToString(app.component());
    EXPECT_TRUE(contains(prompt, "Download base.en (142 MiB)?")) << prompt;

    // Esc goes back to the list without downloading
    app.component()->OnEvent(ftxui::Event::Escape);
    EXPECT_FALSE(app.modelAwaitingDownload().has_value());
    EXPECT_TRUE(app.modelPickerOpen());

    // When: confirmed with Download
    app.component()->OnEvent(ftxui::Event::Return);
    app.component()->OnEvent(ftxui::Event::Return);

    // Then: it runs as a step and fills in the model when done
    EXPECT_FALSE(app.modelPickerOpen());
    ASSERT_TRUE(app.runner().isRunning() || app.runner().state() != StepRunner::State::Idle);
    ASSERT_TRUE(ui.waitUntilFinished(app.runner()));
    EXPECT_EQ(app.runner().state(), StepRunner::State::Succeeded) << app.runner().error();
    EXPECT_EQ(app.runner().name(), "Download base.en");
    EXPECT_DOUBLE_EQ(app.runner().progress(), 1.0);
    const std::string expected = (whisper.models / "ggml-base.en.bin").string();
    EXPECT_EQ(app.form().project().model.base, expected);
    EXPECT_TRUE(app.whisperModels()[1].installed);
    const auto& log = app.runner().log();
    EXPECT_TRUE(std::any_of(log.begin(), log.end(), [](const std::string& l) { return contains(l, "Done!"); }));
    // Progress lines update the gauge rather than filling the log
    EXPECT_FALSE(std::any_of(log.begin(), log.end(), [](const std::string& l) { return contains(l, "71.0M"); }));
}

TEST(ModelPickerTest, FailedDownloadRemovesPartialFile) {
    FakeWhisperCpp whisper;
    whisper.setMode("fail");
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.whisperDir = whisper.root.string();
    app.refreshWhisperModels();

    ASSERT_TRUE(app.downloadModel("medium"));
    ASSERT_TRUE(ui.waitUntilFinished(app.runner()));

    EXPECT_EQ(app.runner().state(), StepRunner::State::Failed);
    EXPECT_TRUE(contains(app.runner().error(), "exit code 1")) << app.runner().error();
    EXPECT_FALSE(std::filesystem::exists(whisper.models / "ggml-medium.bin"));
    EXPECT_EQ(app.form().project().model.base, "");
}

TEST(ModelPickerTest, CancelStopsDownloadAndRemovesPartialFile) {
    FakeWhisperCpp whisper;
    whisper.setMode("slow");
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.whisperDir = whisper.root.string();
    app.refreshWhisperModels();
    ASSERT_TRUE(app.downloadModel("medium"));

    // When: cancelled once the partial file appears
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!std::filesystem::exists(whisper.models / "ggml-medium.bin") &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    app.runner().cancel();

    // Then: done well before the script's 30 s sleep
    ASSERT_TRUE(ui.waitUntilFinished(app.runner()));
    EXPECT_EQ(app.runner().state(), StepRunner::State::Cancelled);
    EXPECT_FALSE(std::filesystem::exists(whisper.models / "ggml-medium.bin"));
}

TEST(ModelPickerTest, DownloadWaitsWhileAnotherStepRuns) {
    FakeWhisperCpp whisper;
    whisper.setMode("slow");
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.whisperDir = whisper.root.string();
    app.refreshWhisperModels();

    ASSERT_TRUE(app.downloadModel("medium"));
    EXPECT_FALSE(app.downloadModel("base.en"));
    EXPECT_FALSE(app.downloadModel("not-a-model"));
    app.runner().cancel();
    ASSERT_TRUE(ui.waitUntilFinished(app.runner()));
}

TEST(ModelPickerTest, PickerExplainsMissingCheckout) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.whisperDir = "/nonexistent/whisper.cpp";

    app.openModelPicker();

    EXPECT_TRUE(app.whisperModels().empty());
    EXPECT_TRUE(contains(renderToString(app.component()), "No models found"));
}

// Saving

namespace {

std::string readAll(const std::filesystem::path& path) {
    std::ifstream in(path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

TEST(SaveProjectTest, SaveWritesProjectThatLoadsBack) {
    // Given: an edited project with transcribed words
    DialogFolder folder;
    const std::filesystem::path path = folder.root / "song.rsans";
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    EXPECT_FALSE(app.hasUnsavedChanges());
    app.form().setFieldText("header.title", "Saved Song");
    app.form().setFieldText("video.width", "1280");
    app.form().setTokens({Token(0, "hello", 0, 500, 0, 1)});
    EXPECT_TRUE(app.hasUnsavedChanges());

    // When
    EXPECT_EQ(app.saveProject(path.string()), "");

    // Then
    EXPECT_FALSE(app.hasUnsavedChanges());
    EXPECT_EQ(app.projectPath(), path.string());
    EXPECT_FALSE(std::filesystem::exists(path.string() + ".tmp"));
    EXPECT_TRUE(contains(renderToString(app.component()), "Saved song.rsans"));

    RsansApp other("ffmpeg", ui.post(), [] {});
    ASSERT_EQ(other.loadProject(path.string()), "");
    EXPECT_EQ(other.form().project().header.title, "Saved Song");
    EXPECT_EQ(other.form().project().video.width, 1280);
    ASSERT_EQ(other.form().project().tokens.size(), 1u);
    EXPECT_EQ(other.form().project().tokens[0].rhymeIndex, 1);
    EXPECT_FALSE(other.hasUnsavedChanges());
}

TEST(SaveProjectTest, HeaderMarksUnsavedChanges) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    EXPECT_TRUE(contains(renderToString(app.component()), "New project  "));

    app.form().setFieldText("header.title", "x");

    EXPECT_TRUE(contains(renderToString(app.component()), "New project *"));
}

TEST(SaveProjectTest, CtrlSOnNewProjectAsksWhereWithNameFromTitle) {
    // Given
    DialogFolder folder;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.form().setFieldText("header.title", "My Song!");

    // When
    app.component()->OnEvent(ftxui::Event::CtrlS);

    // Then: a save dialog, named after the title
    ASSERT_TRUE(app.fileDialogOpen());
    EXPECT_TRUE(app.fileBrowser()->saveMode());
    EXPECT_EQ(app.fileQuery, "my-song.rsans");

    // When: saved in the test folder, without typing the extension
    app.fileQuery.clear();
    type(app, folder.root.string() + "/first");
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_EQ(app.projectPath(), (folder.root / "first.rsans").string());
    EXPECT_TRUE(std::filesystem::exists(folder.root / "first.rsans"));

    // And Ctrl+S now saves straight to it
    app.form().setFieldText("header.title", "Changed");
    app.component()->OnEvent(ftxui::Event::CtrlS);
    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_FALSE(app.hasUnsavedChanges());
    EXPECT_TRUE(contains(readAll(folder.root / "first.rsans"), "Changed"));
}

TEST(SaveProjectTest, SaveAsAsksBeforeReplacingAnotherFile) {
    // Given: old.rsans is in the folder already
    DialogFolder folder;
    std::ofstream(folder.root / "old.rsans") << "x";
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.saveAs();
    app.fileQuery.clear();
    type(app, (folder.root / "old.rsans").string());

    // When: Enter once
    app.component()->OnEvent(ftxui::Event::Return);

    // Then: it asks
    EXPECT_TRUE(app.fileDialogOpen());
    EXPECT_TRUE(contains(renderToString(app.component()), "already exists")) << renderToString(app.component());
    EXPECT_EQ(readAll(folder.root / "old.rsans"), "x");

    // When: Enter again
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_TRUE(contains(readAll(folder.root / "old.rsans"), "\"header\""));
}

TEST(SaveProjectTest, SaveReportsUnwritableFolder) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();

    EXPECT_NE(app.saveProject("/nonexistent/folder/song.rsans"), "");
    EXPECT_EQ(app.projectPath(), "");
}

TEST(SaveProjectTest, CtrlCWithUnsavedChangesAsksFirst) {
    // Given
    ManualPost ui;
    bool quit = false;
    RsansApp app("ffmpeg", ui.post(), [&] { quit = true; });
    app.newProject();
    app.form().setFieldText("header.title", "x");

    // When: Ctrl+C asks instead of quitting
    app.component()->OnEvent(ftxui::Event::CtrlC);
    EXPECT_FALSE(quit);
    ASSERT_TRUE(app.quitPromptOpen());
    EXPECT_TRUE(contains(renderToString(app.component()), "unsaved changes"));

    // Esc goes back
    app.component()->OnEvent(ftxui::Event::Escape);
    EXPECT_FALSE(app.quitPromptOpen());
    EXPECT_FALSE(quit);

    // A second Ctrl+C at the prompt quits anyway
    app.component()->OnEvent(ftxui::Event::CtrlC);
    app.component()->OnEvent(ftxui::Event::CtrlC);
    EXPECT_TRUE(quit);
}

TEST(SaveProjectTest, SaveAndQuitSavesThenQuits) {
    // Given: a project with a file, then changed
    DialogFolder folder;
    ManualPost ui;
    bool quit = false;
    RsansApp app("ffmpeg", ui.post(), [&] { quit = true; });
    app.newProject();
    ASSERT_EQ(app.saveProject((folder.root / "song.rsans").string()), "");
    app.form().setFieldText("header.title", "Last Change");

    // When: Ctrl+C, then Enter on "Save and quit"
    app.component()->OnEvent(ftxui::Event::CtrlC);
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_TRUE(quit);
    EXPECT_TRUE(contains(readAll(folder.root / "song.rsans"), "Last Change"));
}

TEST(SaveProjectTest, CancelledSaveAndQuitDoesNotQuitOnLaterSave) {
    // Given: Save and quit on a new project opens Save as, which is cancelled
    DialogFolder folder;
    ManualPost ui;
    bool quit = false;
    RsansApp app("ffmpeg", ui.post(), [&] { quit = true; });
    app.newProject();
    app.form().setFieldText("header.title", "x");
    app.component()->OnEvent(ftxui::Event::CtrlC);
    app.component()->OnEvent(ftxui::Event::Return);
    ASSERT_TRUE(app.fileDialogOpen());
    app.component()->OnEvent(ftxui::Event::Escape);

    // When: an ordinary save later
    app.saveAs();
    app.fileQuery.clear();
    type(app, (folder.root / "later.rsans").string());
    app.component()->OnEvent(ftxui::Event::Return);

    // Then: saved, still running
    EXPECT_TRUE(std::filesystem::exists(folder.root / "later.rsans"));
    EXPECT_FALSE(quit);
}

TEST(SaveProjectTest, CtrlCQuitsStraightAwayWhenSaved) {
    ManualPost ui;
    bool quit = false;
    RsansApp app("ffmpeg", ui.post(), [&] { quit = true; });
    app.newProject();

    app.component()->OnEvent(ftxui::Event::CtrlC);

    EXPECT_TRUE(quit);
    EXPECT_FALSE(app.quitPromptOpen());
}

TEST(SaveProjectTest, SaveAsUsesRsansExtension) {
    DialogFolder folder;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();

    // name -> path saved
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"plain", "plain.rsans"},
        {"song.json", "song.rsans"},
        {"my.song", "my.song.rsans"},
        {"done.rsans", "done.rsans"},
    };
    for (const auto& [typed, saved] : cases) {
        app.saveAs();
        app.fileQuery.clear();
        type(app, (folder.root / typed).string());
        app.component()->OnEvent(ftxui::Event::Return);

        EXPECT_FALSE(app.fileDialogOpen()) << typed;
        EXPECT_EQ(app.projectPath(), (folder.root / saved).string()) << typed;
        EXPECT_TRUE(std::filesystem::exists(folder.root / saved)) << typed;
    }
    EXPECT_FALSE(std::filesystem::exists(folder.root / "song.json"));
}

TEST(SaveProjectTest, LoadListsRsansAndOlderJsonProjects) {
    // Given: a project of each kind, plus a file that isn't one
    DialogFolder folder;
    {
        ManualPost ui;
        RsansApp saver("ffmpeg", ui.post(), [] {});
        saver.newProject();
        ASSERT_EQ(saver.saveProject((folder.root / "new.rsans").string()), "");
        ASSERT_EQ(saver.saveProject((folder.root / "old.json").string()), "");
    }
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});

    // When: Load project, in the test folder
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::Return);
    ASSERT_TRUE(app.fileDialogOpen());
    app.fileQuery.clear();
    type(app, folder.root.string() + "/");
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    std::vector<std::string> names;
    for (const FileBrowser::Entry& entry : app.fileBrowser()->entries()) {
        names.push_back(entry.name);
    }
    EXPECT_NE(std::find(names.begin(), names.end(), "new.rsans"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "old.json"), names.end());
    EXPECT_EQ(std::find(names.begin(), names.end(), "notes.txt"), names.end());
    EXPECT_EQ(app.loadProject((folder.root / "old.json").string()), "");
}

// Back to the start menu

TEST(MenuTest, EscapeGoesToMenuWhenSavedAndNewProjectStartsFresh) {
    // Given
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    EXPECT_TRUE(contains(renderToString(app.component()), "Menu"));

    // When
    app.component()->OnEvent(ftxui::Event::Escape);

    // Then: the start screen, with New project highlighted
    EXPECT_EQ(app.screen(), RsansApp::Screen::Start);
    app.component()->OnEvent(ftxui::Event::Return);
    EXPECT_EQ(app.screen(), RsansApp::Screen::Editor);
}

TEST(MenuTest, MenuButtonIsAtEndOfHeader) {
    // Given: focus on the tab bar (Project)
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();

    // When: right past the tabs, Save and Save as, to Menu
    for (int i = 0; i < 5; ++i) {
        app.component()->OnEvent(ftxui::Event::ArrowRight);
    }
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_EQ(app.screen(), RsansApp::Screen::Start);
}

TEST(MenuTest, EscapeClosesOpenDialogInsteadOfLeaving) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.openFontPicker(RsansApp::FontTarget::Header);

    app.component()->OnEvent(ftxui::Event::Escape);

    EXPECT_FALSE(app.fontPickerOpen());
    EXPECT_EQ(app.screen(), RsansApp::Screen::Editor);
}

TEST(MenuTest, UnsavedChangesAskBeforeGoingToMenu) {
    // Given
    ManualPost ui;
    bool quit = false;
    RsansApp app("ffmpeg", ui.post(), [&] { quit = true; });
    app.newProject();
    app.form().setFieldText("header.title", "Draft");

    // When
    EXPECT_TRUE(app.goToMenu());

    // Then: asked, not gone
    ASSERT_TRUE(app.quitPromptOpen());
    EXPECT_EQ(app.screen(), RsansApp::Screen::Editor);
    EXPECT_TRUE(contains(renderToString(app.component()), "going back to the menu"));

    // Cancel stays
    app.component()->OnEvent(ftxui::Event::Escape);
    EXPECT_FALSE(app.quitPromptOpen());
    EXPECT_EQ(app.screen(), RsansApp::Screen::Editor);

    // Don't save goes to the menu, discarding the change
    app.goToMenu();
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    app.component()->OnEvent(ftxui::Event::Return);
    EXPECT_EQ(app.screen(), RsansApp::Screen::Start);
    EXPECT_FALSE(quit);
    app.newProject();
    EXPECT_EQ(app.form().project().header.title, "");
}

TEST(MenuTest, SaveFromMenuPromptSavesThenGoesToMenu) {
    // Given: a new project, so saving asks where
    DialogFolder folder;
    ManualPost ui;
    bool quit = false;
    RsansApp app("ffmpeg", ui.post(), [&] { quit = true; });
    app.newProject();
    app.form().setFieldText("header.title", "Draft");
    app.goToMenu();

    // When: Save, then a name in the save dialog
    app.component()->OnEvent(ftxui::Event::Return);
    ASSERT_TRUE(app.fileDialogOpen());
    app.fileQuery.clear();
    type(app, (folder.root / "draft").string());
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_TRUE(std::filesystem::exists(folder.root / "draft.rsans"));
    EXPECT_EQ(app.screen(), RsansApp::Screen::Start);
    EXPECT_FALSE(quit);

    // And a later ordinary save doesn't leave
    ASSERT_EQ(app.loadProject((folder.root / "draft.rsans").string()), "");
    app.form().setFieldText("header.title", "Draft 2");
    app.save();
    EXPECT_EQ(app.screen(), RsansApp::Screen::Editor);
}

TEST(MenuTest, MenuWaitsWhileStepRuns) {
    // Given: a step running (a model download that hangs)
    FakeWhisperCpp whisper;
    whisper.setMode("slow");
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.whisperDir = whisper.root.string();
    app.refreshWhisperModels();
    ASSERT_TRUE(app.downloadModel("medium"));

    // When / Then
    EXPECT_FALSE(app.goToMenu());
    app.component()->OnEvent(ftxui::Event::Escape);
    EXPECT_EQ(app.screen(), RsansApp::Screen::Editor);
    EXPECT_FALSE(app.quitPromptOpen());

    app.runner().cancel();
    ASSERT_TRUE(ui.waitUntilFinished(app.runner()));
    EXPECT_TRUE(app.goToMenu());
    EXPECT_EQ(app.screen(), RsansApp::Screen::Start);
}

// Dialog frames

namespace {

// Where a dialog's top-left corner is: the "╭" that isn't the app's own (0, 0)
std::pair<int, int> dialogCorner(const ftxui::Screen& screen) {
    for (int y = 1; y < screen.dimy(); ++y) {
        for (int x = 1; x < screen.dimx(); ++x) {
            if (screen.PixelAt(x, y).character == "╭") {
                return {x, y};
            }
        }
    }
    return {-1, -1};
}

} // namespace

TEST(DialogFrameTest, DialogFramesAreYellowButTitlesAndAppFrameAreNot) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.setFonts(pickerFonts());

    const auto check = [&](const std::string& which) {
        auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(60));
        ftxui::Render(screen, app.component()->Render());
        const auto [x, y] = dialogCorner(screen);
        ASSERT_GE(x, 0) << which;
        const ftxui::Color yellow(ftxui::Color::YellowLight);
        EXPECT_EQ(screen.PixelAt(x, y).foreground_color, yellow) << which;      // ╭
        EXPECT_EQ(screen.PixelAt(x, y + 1).foreground_color, yellow) << which;  // │
        EXPECT_NE(screen.PixelAt(x + 2, y).foreground_color, yellow) << which;  // Title
        EXPECT_NE(screen.PixelAt(0, 0).foreground_color, yellow) << which;      // The app
        // Separator rows are yellow all the way across
        for (int row = y + 1; row < screen.dimy(); ++row) {
            if (screen.PixelAt(x, row).character == "├") {
                EXPECT_EQ(screen.PixelAt(x + 5, row).foreground_color, yellow) << which;
                break;
            }
        }
    };

    app.openFontPicker(RsansApp::FontTarget::Header);
    check("font picker");
    app.component()->OnEvent(ftxui::Event::Escape);

    app.browseFor("audio.path");
    check("file dialog");
    app.component()->OnEvent(ftxui::Event::Escape);

    app.openModelPicker();
    check("model picker");
    app.component()->OnEvent(ftxui::Event::Escape);

    app.form().setFieldText("header.title", "x");
    app.goToMenu();
    check("unsaved changes");
}

TEST(RsansAppTest, EditorContentIsPaddedFromBorder) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();

    const std::string screen = renderToString(app.component());

    // Two spaces between the border and the text, on the left and the right
    EXPECT_TRUE(contains(screen, "│  Song")) << screen;
    EXPECT_TRUE(contains(screen, "│    Title")) << screen;
    EXPECT_TRUE(contains(screen, "│  Arrows/Tab: move")) << screen;
    // Right: the button's own space and the scroll column, then the padding
    EXPECT_TRUE(contains(screen, "Browse    │")) << screen;
}

// Correcting words

TEST(ProjectFormTest, SetTokenTextTrimsAndRejectsEmpty) {
    ProjectForm form(defaultProject());
    form.setTokens({Token(0, "wrold", 0, 400, 0, 2), Token(1, "again", 400, 800, 0, std::nullopt)});

    EXPECT_TRUE(form.setTokenText(0, "  world "));
    EXPECT_EQ(form.project().tokens[0].text, "world");
    EXPECT_EQ(form.project().tokens[0].startMs, 0);       // Timing kept
    EXPECT_EQ(form.project().tokens[0].rhymeIndex, 2);    // Until rhymes are detected again

    EXPECT_FALSE(form.setTokenText(0, "   "));
    EXPECT_FALSE(form.setTokenText(5, "x"));
    EXPECT_EQ(form.project().tokens[0].text, "world");

    EXPECT_TRUE(form.removeToken(0));
    ASSERT_EQ(form.project().tokens.size(), 1u);
    EXPECT_EQ(form.project().tokens[0].text, "again");
    EXPECT_FALSE(form.removeToken(1));
}

namespace {

// An app on the Lyrics tab with the word list focused, holding
// line 0: "hello wrold", line 1: "again"
void openLyrics(RsansApp& app) {
    app.newProject();
    app.form().setTokens({Token(0, "hello", 0, 400, 0, std::nullopt),
                          Token(1, "wrold", 400, 800, 0, std::nullopt),
                          Token(2, "again", 1000, 1400, 1, std::nullopt)});
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    app.component()->OnEvent(ftxui::Event::ArrowDown);
}

} // namespace

TEST(WordEditorTest, EnterOnWordEditsItAndMarksRhymesOutOfDate) {
    // Given
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    openLyrics(app);
    EXPECT_EQ(app.selectedWord(), 0u);

    // When: right to "wrold", Enter, retype it
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    EXPECT_EQ(app.selectedWord(), 1u);
    app.component()->OnEvent(ftxui::Event::Return);
    ASSERT_TRUE(app.wordEditorOpen());
    EXPECT_EQ(app.wordEditText, "wrold");
    EXPECT_TRUE(contains(renderToString(app.component()), "Edit word"));
    for (int i = 0; i < 5; ++i) {
        app.component()->OnEvent(ftxui::Event::Backspace);
    }
    type(app, "world");
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_FALSE(app.wordEditorOpen());
    EXPECT_EQ(app.form().project().tokens[1].text, "world");
    EXPECT_TRUE(app.rhymesOutOfDate());
    EXPECT_TRUE(app.hasUnsavedChanges());
    const std::string screen = renderToString(app.component());
    EXPECT_TRUE(contains(screen, "hello world")) << screen;
    EXPECT_TRUE(contains(screen, "run Detect rhymes again")) << screen;
}

TEST(WordEditorTest, ArrowsMoveBetweenWordsAndLines) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    openLyrics(app);

    app.component()->OnEvent(ftxui::Event::ArrowRight);  // wrold
    app.component()->OnEvent(ftxui::Event::ArrowDown);   // again (the line below has one word)
    EXPECT_EQ(app.selectedWord(), 2u);
    app.component()->OnEvent(ftxui::Event::ArrowRight);  // Already the last word
    EXPECT_EQ(app.selectedWord(), 2u);
    app.component()->OnEvent(ftxui::Event::ArrowUp);     // hello (first place in the line above)
    EXPECT_EQ(app.selectedWord(), 0u);
    app.component()->OnEvent(ftxui::Event::ArrowLeft);   // Already the first word
    EXPECT_EQ(app.selectedWord(), 0u);
    EXPECT_TRUE(contains(renderToString(app.component()), "\"hello\" at 0:00"));
}

TEST(WordEditorTest, EmptyWordIsRefusedAndEscapeCancels) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    openLyrics(app);
    app.component()->OnEvent(ftxui::Event::Return);

    // When: cleared and saved
    for (int i = 0; i < 5; ++i) {
        app.component()->OnEvent(ftxui::Event::Backspace);
    }
    app.component()->OnEvent(ftxui::Event::Return);

    // Then: refused, still open
    EXPECT_TRUE(app.wordEditorOpen());
    EXPECT_TRUE(contains(renderToString(app.component()), "Delete word to remove it"));

    // Esc leaves it unchanged, and stays in the editor rather than going to the menu
    app.component()->OnEvent(ftxui::Event::Escape);
    EXPECT_FALSE(app.wordEditorOpen());
    EXPECT_EQ(app.screen(), RsansApp::Screen::Editor);
    EXPECT_EQ(app.form().project().tokens[0].text, "hello");
    EXPECT_FALSE(app.rhymesOutOfDate());
}

TEST(WordEditorTest, DeleteWordRemovesIt) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    openLyrics(app);
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    app.component()->OnEvent(ftxui::Event::Return);

    // When: down past the rhyme groups (None, New group) to the buttons, right to Delete word
    for (int i = 0; i < 3; ++i) {
        app.component()->OnEvent(ftxui::Event::ArrowDown);
    }
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_FALSE(app.wordEditorOpen());
    ASSERT_EQ(app.form().project().tokens.size(), 2u);
    EXPECT_EQ(app.form().project().tokens[1].text, "again");
    EXPECT_EQ(app.selectedWord(), 1u);
    EXPECT_TRUE(app.rhymesOutOfDate());
}

TEST(WordEditorTest, EditingWaitsWhileStepRuns) {
    FakeWhisperCpp whisper;
    whisper.setMode("slow");
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    openLyrics(app);
    app.whisperDir = whisper.root.string();
    app.refreshWhisperModels();
    ASSERT_TRUE(app.downloadModel("medium"));

    EXPECT_FALSE(app.openWordEditor());
    EXPECT_TRUE(contains(renderToString(app.component()), "Editing waits until"));

    app.runner().cancel();
    ASSERT_TRUE(ui.waitUntilFinished(app.runner()));
    EXPECT_TRUE(app.openWordEditor());
}

TEST(WordEditorTest, DetectingRhymesClearsOutOfDateNote) {
    // Given: a corrected word
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    openLyrics(app);
    const std::filesystem::path dict = std::filesystem::temp_directory_path() / "rsans_word_test.dict";
    std::ofstream(dict) << "hello HH AH0 L OW1\nworld W ER1 L D\nagain AH0 G EH1 N\n";
    app.form().setFieldText("cmudict", dict.string());
    app.selectWord(1);
    ASSERT_TRUE(app.openWordEditor());
    app.wordEditText = "world";
    app.component()->OnEvent(ftxui::Event::Return);
    ASSERT_TRUE(app.rhymesOutOfDate());

    // When
    ASSERT_TRUE(app.runStep(RsansApp::Step::Rhyme));
    ASSERT_TRUE(ui.waitUntilFinished(app.runner()));

    // Then: the corrected word was used
    EXPECT_FALSE(app.rhymesOutOfDate());
    EXPECT_EQ(app.form().project().tokens[1].text, "world");
    std::filesystem::remove(dict);
}

// Rhyme groups by hand

TEST(ProjectFormTest, SetTokenRhymeSetsAndClearsGroup) {
    ProjectForm form(defaultProject());
    form.setTokens({Token(0, "night", 0, 400, 0, std::nullopt)});

    EXPECT_TRUE(form.setTokenRhyme(0, 3));
    EXPECT_EQ(form.project().tokens[0].rhymeIndex, 3);
    EXPECT_TRUE(form.setTokenRhyme(0, std::nullopt));
    EXPECT_FALSE(form.project().tokens[0].rhymeIndex.has_value());
    EXPECT_FALSE(form.setTokenRhyme(0, -1));
    EXPECT_FALSE(form.setTokenRhyme(1, 0));
}

namespace {

// Lyrics with two rhyme groups: 0 = night/light, 1 = sign/line; "neon" has none
void openRhymingLyrics(RsansApp& app) {
    app.newProject();
    app.form().setTokens({Token(0, "night", 0, 400, 0, 0),
                          Token(1, "neon", 400, 800, 0, std::nullopt),
                          Token(2, "light", 800, 1200, 0, 0),
                          Token(3, "sign", 1400, 1800, 1, 1),
                          Token(4, "line", 1800, 2200, 1, 1)});
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    app.component()->OnEvent(ftxui::Event::ArrowDown);
}

} // namespace

TEST(RhymeGroupTest, EditorListsGroupsWithTheirWords) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    openRhymingLyrics(app);

    // When: editing "night" (group 1)
    ASSERT_TRUE(app.openWordEditor());

    // Then: its own word isn't listed in its group
    EXPECT_EQ(app.rhymeGroupLabels(),
              (std::vector<std::string>{"None", "1  light", "2  sign, line", "New group"}));
    const std::string screen = renderToString(app.component());
    EXPECT_TRUE(contains(screen, "Rhyme group")) << screen;
    EXPECT_TRUE(contains(screen, "> \u25a0 1  light")) << screen;  // Its group is the current one
}

TEST(RhymeGroupTest, ChoosingGroupMovesWordWithoutOutOfDateNote) {
    // Given: "neon" has no group
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    openRhymingLyrics(app);
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    app.component()->OnEvent(ftxui::Event::Return);
    ASSERT_TRUE(app.wordEditorOpen());

    // When: down into the groups (None), down twice to "2  sign, line", Enter
    for (int i = 0; i < 3; ++i) {
        app.component()->OnEvent(ftxui::Event::ArrowDown);
    }
    app.component()->OnEvent(ftxui::Event::Return);

    // Then
    EXPECT_FALSE(app.wordEditorOpen());
    EXPECT_EQ(app.form().project().tokens[1].rhymeIndex, 1);
    EXPECT_FALSE(app.rhymesOutOfDate());
    EXPECT_TRUE(app.hasUnsavedChanges());
}

TEST(RhymeGroupTest, NoneAndNewGroup) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    openRhymingLyrics(app);

    // "night" to None: up from its group to None
    ASSERT_TRUE(app.openWordEditor());
    app.component()->OnEvent(ftxui::Event::ArrowDown);  // Into the groups, on its own (1)
    app.component()->OnEvent(ftxui::Event::ArrowUp);    // None
    app.component()->OnEvent(ftxui::Event::Return);
    EXPECT_FALSE(app.form().project().tokens[0].rhymeIndex.has_value());

    // "neon" to a new group: the lowest number not in use (groups 0 and 1 are)
    app.selectWord(1);
    ASSERT_TRUE(app.openWordEditor());
    for (int i = 0; i < 4; ++i) {
        app.component()->OnEvent(ftxui::Event::ArrowDown);  // None, 1, 2, New group
    }
    app.component()->OnEvent(ftxui::Event::Return);
    EXPECT_EQ(app.form().project().tokens[1].rhymeIndex, 2);
}

TEST(RhymeGroupTest, NewGroupReusesFreedNumber) {
    // Given: group 0 is emptied
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    openRhymingLyrics(app);
    app.form().setTokenRhyme(0, std::nullopt);
    app.form().setTokenRhyme(2, std::nullopt);

    // When: "neon" goes in a new group
    app.selectWord(1);
    ASSERT_TRUE(app.openWordEditor());
    EXPECT_EQ(app.rhymeGroupLabels().back(), "New group");
    for (int i = 0; i < 3; ++i) {
        app.component()->OnEvent(ftxui::Event::ArrowDown);  // None, 2, New group
    }
    app.component()->OnEvent(ftxui::Event::Return);

    // Then: 0 is free again
    EXPECT_EQ(app.form().project().tokens[1].rhymeIndex, 0);
}

TEST(RhymeGroupTest, TextAndGroupChangeTogetherAndCancelChangesNeither) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    openRhymingLyrics(app);

    // Esc: nothing changes
    ASSERT_TRUE(app.openWordEditor());
    type(app, "s");
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::ArrowUp);
    app.component()->OnEvent(ftxui::Event::ArrowUp);
    app.component()->OnEvent(ftxui::Event::Escape);
    EXPECT_EQ(app.form().project().tokens[0].text, "night");
    EXPECT_EQ(app.form().project().tokens[0].rhymeIndex, 0);

    // Save: both change
    ASSERT_TRUE(app.openWordEditor());
    type(app, "s");
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::ArrowDown);  // 2  sign, line
    app.component()->OnEvent(ftxui::Event::Return);
    EXPECT_EQ(app.form().project().tokens[0].text, "nights");
    EXPECT_EQ(app.form().project().tokens[0].rhymeIndex, 1);
}

// Saving the video

namespace {

const char* const dejaVuFont = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

// Fills in everything Export needs, with files in folder. False if the font
// isn't installed.
bool makeExportReady(RsansApp& app, const std::filesystem::path& folder, const std::string& title) {
    if (!std::filesystem::exists(dejaVuFont)) {
        return false;
    }
    app.newProject();
    writeTestBmp((folder / "cover.bmp").string());
    const std::string audio = writeTestWav("rsans_video_test.wav", 1, 16000, 500);
    app.form().setFieldText("header.title", title);
    app.form().setFieldText("header.artist", "Artist");
    app.form().setFieldText("header.media", (folder / "cover.bmp").string());
    app.form().setFieldText("audio.path", audio);
    for (const char* key : {"header.fontName", "layout.fontName"}) {
        app.form().setFieldText(key, "DejaVu Sans");
    }
    for (const char* key : {"header.fontPath", "layout.fontPath"}) {
        app.form().setFieldText(key, dejaVuFont);
    }
    app.form().setFieldText("video.width", "320");
    app.form().setFieldText("video.height", "240");
    app.form().setTokens({Token(0, "hello", 0, 400, 0, std::nullopt)});
    return app.problemsFor(RsansApp::Step::Export).empty();
}

} // namespace

TEST(VideoSaveTest, ExportAsksWhereToSaveNamedAfterProject) {
    DialogFolder folder;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    if (!makeExportReady(app, folder.root, "My Song!")) {
        GTEST_SKIP() << "DejaVu Sans not installed";
    }

    // Unsaved: named after the title, in the working folder
    ASSERT_TRUE(app.runStep(RsansApp::Step::Export));
    ASSERT_TRUE(app.fileDialogOpen());
    EXPECT_TRUE(app.fileBrowser()->saveMode());
    EXPECT_EQ(app.fileQuery, "my-song.mp4");
    EXPECT_EQ(app.fileBrowser()->directory(), std::filesystem::current_path());
    EXPECT_FALSE(app.runner().isRunning());
    EXPECT_TRUE(contains(renderToString(app.component()), "Save video as"));
    app.component()->OnEvent(ftxui::Event::Escape);

    // Saved: named after the project file, in its folder
    ASSERT_EQ(app.saveProject((folder.root / "night-drive.rsans").string()), "");
    ASSERT_TRUE(app.runStep(RsansApp::Step::Export));
    EXPECT_EQ(app.fileQuery, "night-drive.mp4");
    EXPECT_EQ(app.fileBrowser()->directory(), folder.root);
    app.component()->OnEvent(ftxui::Event::Escape);

    // No title or file: "video.mp4"
    app.newProject();
    EXPECT_EQ(std::filesystem::path(app.videoPathSuggestion()).filename(), "video.mp4");
}

TEST(VideoSaveTest, CancellingTheDialogRunsNothing) {
    DialogFolder folder;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    if (!makeExportReady(app, folder.root, "Song")) {
        GTEST_SKIP() << "DejaVu Sans not installed";
    }

    ASSERT_TRUE(app.runStep(RsansApp::Step::Export));
    app.component()->OnEvent(ftxui::Event::Escape);

    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_EQ(app.runner().state(), StepRunner::State::Idle);
}

TEST(VideoSaveTest, NotReadyMeansNoDialog) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();

    EXPECT_FALSE(app.runStep(RsansApp::Step::Export));
    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_FALSE(app.startVideoStep(RsansApp::Step::Export, "out.mp4"));
}

TEST(VideoSaveTest, DialogRefusesSubtitleAndProjectNamesAndAsksBeforeReplacing) {
    // Given: old.mp4 exists
    DialogFolder folder;
    std::ofstream(folder.root / "old.mp4") << "x";
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    if (!makeExportReady(app, folder.root, "Song")) {
        GTEST_SKIP() << "DejaVu Sans not installed";
    }
    const auto submit = [&](const std::string& name) {
        app.fileQuery.clear();
        type(app, (folder.root / name).string());
        app.component()->OnEvent(ftxui::Event::Return);
    };
    ASSERT_TRUE(app.runStep(RsansApp::Step::Export));

    submit("lyrics.ass");
    EXPECT_TRUE(app.fileDialogOpen());
    EXPECT_TRUE(contains(renderToString(app.component()), "can't be a .ass file"));
    submit("song.rsans");
    EXPECT_TRUE(app.fileDialogOpen());

    // Replacing needs a second Enter
    submit("old.mp4");
    EXPECT_TRUE(app.fileDialogOpen());
    EXPECT_TRUE(contains(renderToString(app.component()), "already exists"));
    EXPECT_FALSE(app.runner().isRunning());
    app.component()->OnEvent(ftxui::Event::Return);
    EXPECT_FALSE(app.fileDialogOpen());
    EXPECT_TRUE(app.runner().isRunning() || app.runner().state() != StepRunner::State::Idle);
    app.runner().cancel();
    ASSERT_TRUE(ui.waitUntilFinished(app.runner()));

    // startVideoStep refuses them too
    EXPECT_FALSE(app.startVideoStep(RsansApp::Step::Export, (folder.root / "x.ass").string()));
    EXPECT_FALSE(app.startVideoStep(RsansApp::Step::Export, ""));
}

TEST(VideoSaveTest, ExportRendersToChosenFileAndRemembersFolder) {
    if (!ffmpegAvailable()) {
        GTEST_SKIP() << "ffmpeg not on PATH";
    }
    // Given: a saved project, and a separate folder for videos
    DialogFolder folder;
    const std::filesystem::path videos = folder.root / "videos";
    std::filesystem::create_directories(videos);
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    if (!makeExportReady(app, folder.root, "Song")) {
        GTEST_SKIP() << "DejaVu Sans not installed";
    }
    ASSERT_EQ(app.saveProject((folder.root / "song.rsans").string()), "");

    // When: Export, saved as videos/clip (no extension)
    ASSERT_TRUE(app.runStep(RsansApp::Step::Export));
    app.fileQuery.clear();
    type(app, (videos / "clip").string());
    app.component()->OnEvent(ftxui::Event::Return);
    ASSERT_FALSE(app.fileDialogOpen());
    ASSERT_TRUE(ui.waitUntilFinished(app.runner()));

    // Then
    EXPECT_EQ(app.runner().state(), StepRunner::State::Succeeded) << app.runner().error();
    EXPECT_TRUE(std::filesystem::exists(videos / "clip.mp4"));
    EXPECT_TRUE(std::filesystem::exists(videos / "clip.ass"));

    // And the next export starts in that folder, named after the project
    EXPECT_EQ(app.videoPathSuggestion(), (videos / "song.mp4").string());
}

// Settings kept between runs

namespace {

struct SettingsFolder {
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("rsans_settings_" + std::to_string(::getpid()));
    std::filesystem::path file = root / "rsans" / "settings.json";
    SettingsFolder() { std::filesystem::remove_all(root); }
    ~SettingsFolder() { std::filesystem::remove_all(root); }
    SettingsFolder(const SettingsFolder&) = delete;
    SettingsFolder& operator=(const SettingsFolder&) = delete;
    SettingsFolder(SettingsFolder&&) = delete;
    SettingsFolder& operator=(SettingsFolder&&) = delete;
};

// Sets an environment variable for one test, putting it back afterwards
class ScopedEnv {
  public:
    ScopedEnv(const char* name, const char* value) : d_name(name) {
        const char* old = std::getenv(name);
        if (old != nullptr) {
            d_old = old;
        }
        if (value != nullptr) {
            ::setenv(name, value, 1);
        } else {
            ::unsetenv(name);
        }
    }
    ~ScopedEnv() {
        if (d_old) {
            ::setenv(d_name, d_old->c_str(), 1);
        } else {
            ::unsetenv(d_name);
        }
    }
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;
    ScopedEnv(ScopedEnv&&) = delete;
    ScopedEnv& operator=(ScopedEnv&&) = delete;

  private:
    const char* d_name;
    std::optional<std::string> d_old;
};

} // namespace

TEST(AppSettingsTest, SavedSettingsAreUsedUnlessGivenForThisRun) {
    SettingsFolder settings;
    ASSERT_EQ(saveSettings(settings.file.string(), {"/opt/ffmpeg", "/opt/whisper.cpp"}), "");
    ScopedEnv noWhisper("WHISPER_CPP_DIR", nullptr);
    ManualPost ui;

    // Nothing given: the saved values
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.useSettingsFile(settings.file.string(), false);
    EXPECT_EQ(app.ffmpegPath, "/opt/ffmpeg");
    EXPECT_EQ(app.whisperDir, "/opt/whisper.cpp");

    // --ffmpeg and $WHISPER_CPP_DIR win
    ScopedEnv whisper("WHISPER_CPP_DIR", "/env/whisper.cpp");
    RsansApp given("/given/ffmpeg", ui.post(), [] {});
    given.whisperDir = "/env/whisper.cpp";  // As defaultWhisperDir() would give
    given.useSettingsFile(settings.file.string(), true);
    EXPECT_EQ(given.ffmpegPath, "/given/ffmpeg");
    EXPECT_EQ(given.whisperDir, "/env/whisper.cpp");
}

TEST(AppSettingsTest, EditingOnRunTabSavesForNextTime) {
    // Given: no settings yet
    SettingsFolder settings;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.useSettingsFile(settings.file.string(), false);
    app.newProject();
    EXPECT_TRUE(contains(renderToString(app.component()), "") );

    // When: the Run tab's FFmpeg box is edited
    app.component()->OnEvent(ftxui::Event::ArrowRight);  // Run tab
    app.component()->OnEvent(ftxui::Event::ArrowDown);   // FFmpeg
    EXPECT_TRUE(contains(renderToString(app.component()), "saved for next time"));
    type(app, "7");

    // Then: written, and read back by the next run
    EXPECT_TRUE(app.settingsError().empty()) << app.settingsError();
    ASSERT_TRUE(std::filesystem::exists(settings.file));
    EXPECT_EQ(loadSettings(settings.file.string()).ffmpegPath, app.ffmpegPath);
    EXPECT_NE(app.ffmpegPath, "ffmpeg");
    RsansApp next("ffmpeg", ui.post(), [] {});
    next.useSettingsFile(settings.file.string(), false);
    EXPECT_EQ(next.ffmpegPath, app.ffmpegPath);
}

TEST(AppSettingsTest, WithoutSettingsFileNothingIsSaved) {
    // Tests (and other embedders) don't touch the real settings file
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    type(app, "7");

    EXPECT_TRUE(app.settingsError().empty());
    EXPECT_FALSE(contains(renderToString(app.component()), "saved for next time"));
}

TEST(AppSettingsTest, OnlyTheEditedSettingIsSaved) {
    // Given: --ffmpeg for this run, and a default whisper.cpp folder
    SettingsFolder settings;
    ScopedEnv noWhisper("WHISPER_CPP_DIR", nullptr);
    ManualPost ui;
    RsansApp app("/given/ffmpeg", ui.post(), [] {});
    app.useSettingsFile(settings.file.string(), true);
    app.newProject();

    // When: only whisper.cpp is edited (Run tab, down past FFmpeg)
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    app.component()->OnEvent(ftxui::Event::ArrowDown);
    type(app, "x");

    // Then: the folder is saved, the one-off --ffmpeg isn't
    const AppSettings saved = loadSettings(settings.file.string());
    EXPECT_EQ(saved.whisperDir, app.whisperDir);
    EXPECT_EQ(saved.ffmpegPath, "");
}

TEST(AppSettingsTest, ChecksDontShowButtonEllipsis) {
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.component()->OnEvent(ftxui::Event::ArrowRight);

    const std::string screen = renderToString(app.component());
    EXPECT_TRUE(contains(screen, "Export...")) << screen;          // The button
    EXPECT_TRUE(contains(screen, "    Export      ")) << screen;   // The check
}

// Font names that don't find their font file

namespace {
const char* const dejaVuSansFile = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
}

TEST(ProjectFormTest, WarnsWhenFontNameDoesntMatchFile) {
    if (!std::filesystem::exists(dejaVuSansFile)) {
        GTEST_SKIP() << "DejaVu Sans not installed";
    }
    ProjectForm form(defaultProject());
    form.setFieldText("layout.fontPath", dejaVuSansFile);

    // A typed name that isn't the file's font
    form.setFieldText("layout.fontName", "Arial");
    EXPECT_NE(form.warningFor("layout.fontName").find("\"DejaVu Sans\""), std::string::npos);
    EXPECT_EQ(form.warnings(), (std::vector<std::string>{"the lyrics font's name doesn't match its file"}));
    EXPECT_EQ(form.warningFor("header.fontName"), "");

    // Fixed
    form.setFieldText("layout.fontName", "dejavu sans");
    EXPECT_EQ(form.warningFor("layout.fontName"), "");
    EXPECT_TRUE(form.warnings().empty());
}

TEST(FontNameWarningTest, ShownOnProjectTabAndInExportCheck) {
    if (!std::filesystem::exists(dejaVuSansFile)) {
        GTEST_SKIP() << "DejaVu Sans not installed";
    }
    // Given: a project loaded with a header font name that isn't its file's
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    app.newProject();
    app.form().setFieldText("header.fontPath", dejaVuSansFile);
    app.form().setFieldText("header.fontName", "Helvetica");

    // Then: under the font, on the Project tab
    const std::string project = renderToString(app.component(), 120, 70);
    EXPECT_TRUE(contains(project, "The file's font is \"DejaVu Sans\", not \"Helvetica\"")) << project;

    // And in the Run tab's checks once Export is otherwise ready (here: not yet)
    app.component()->OnEvent(ftxui::Event::ArrowRight);
    const std::string run = renderToString(app.component(), 120, 50);
    EXPECT_FALSE(contains(run, "ready, but")) << run;
}

TEST(FontNameWarningTest, ReadyExportCheckMentionsMismatchedFont) {
    DialogFolder folder;
    ManualPost ui;
    RsansApp app("ffmpeg", ui.post(), [] {});
    if (!makeExportReady(app, folder.root, "Song")) {
        GTEST_SKIP() << "DejaVu Sans not installed";
    }
    app.form().setFieldText("layout.fontName", "Helvetica");
    ASSERT_TRUE(app.problemsFor(RsansApp::Step::Export).empty());  // A warning, not a blocker

    app.component()->OnEvent(ftxui::Event::ArrowRight);
    const std::string run = renderToString(app.component(), 140, 50);

    EXPECT_TRUE(contains(run, "ready, but the lyrics font's name doesn't match its file (see Project)")) << run;
}
