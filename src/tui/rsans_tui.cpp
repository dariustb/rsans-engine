#include <rsans_tui.h>

#include <rsans_export.h>
#include <rsans_rhyme.h>
#include <rsans_whisper.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
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
: ffmpegPath(std::move(ffmpeg))
, d_form(defaultProject())
, d_quit(std::move(quit))
, d_post(post)
, d_tabNames({"Project", "Run", "Lyrics"})
, d_runner(std::move(post)) {
    Component tabs = Container::Tab({makeProjectTab(), makeRunTab(), makeLyricsTab()}, &d_tab);
    Component tabBar = Toggle(&d_tabNames, &d_tab);
    Component layout = Container::Vertical({tabBar, tabs});

    Component rendered = Renderer(layout, [this, tabBar, tabs] {
        return vbox({
            hbox({text(" RSANS ") | bold | inverted, text(" "), tabBar->Render(), filler(),
                  text(stateText(d_runner) + " ") | dim}),
            separator(),
            tabs->Render() | flex,
            separator(),
            text(" Arrows/Tab: move   Enter: press   Ctrl+C: quit") | dim,
        }) | border;
    });

    // The font picker opens on top of everything else
    Component withPicker = Modal(rendered, makeFontPicker(), &d_pickerOpen);

    d_root = CatchEvent(withPicker, [this](const Event& event) {
        if (event == Event::CtrlC) {
            d_runner.cancel();  // The runner's destructor waits for it to stop
            d_quit();
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
    Component cancel = Button("Cancel", [this] { d_pickerOpen = false; }, ButtonOption::Ascii());

    Component container = Container::Vertical({search, list, Container::Horizontal({rescan, cancel})});

    Component rendered = Renderer(container, [this, search, list, rescan, cancel] {
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

        return window(text(title), vbox({
            hbox({text("Search ") | bold, search->Render() | flex}),
            text(status) | dim,
            separator(),
            list->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 14),
            separator(),
            text(selectedPath.empty() ? " " : selectedPath) | dim,
            hbox({rescan->Render(), text(" "), cancel->Render(), filler(),
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

Component RsansApp::makeProjectTab() {
    Components inputs;
    std::vector<std::pair<FormField*, Component>> rows;
    for (FormField& field : d_form.fields()) {
        // Each font is one row: a button opening the picker, which fills in
        // both the font's name and its file
        const bool isFontName = field.key == "header.fontName" || field.key == "layout.fontName";
        const bool isFontPath = field.key == "header.fontPath" || field.key == "layout.fontPath";
        if (isFontPath) {
            continue;
        }
        if (isFontName) {
            const FontTarget target = field.key == "header.fontName" ? FontTarget::Header : FontTarget::Layout;
            std::string* label = target == FontTarget::Header ? &d_headerFontLabel : &d_layoutFontLabel;
            Component button = Button(label, [this, target] { openFontPicker(target); }, ButtonOption::Ascii());
            inputs.push_back(button);
            rows.emplace_back(&field, button);
            continue;
        }

        InputOption option = singleLineInput();
        option.on_change = [this] { d_form.apply(); };
        option.placeholder = field.kind == FieldKind::Path ? "path to file" : "";
        Component input = TextInput(&field.text, option);
        inputs.push_back(input);
        rows.emplace_back(&field, input);
    }

    Component container = Container::Vertical(inputs);
    return Renderer(container, [this, rows] {
        Elements lines;
        std::string section;
        for (const auto& [field, input] : rows) {
            if (field->section != section) {
                section = field->section;
                if (!lines.empty()) {
                    lines.push_back(text(""));
                }
                lines.push_back(text(section) | bold);
            }
            std::vector<std::string> errors = d_form.errorsFor(field->key);

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
                continue;
            }
            Element row = hbox({
                text("  " + field->label) | size(WIDTH, EQUAL, labelWidth),
                text(errors.empty() ? "  " : "! ") | color(ftxui::Color::Red),
                input->Render() | flex,
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
    const bool writesVideo = step == Step::Export || step == Step::Full;
    if (writesVideo) {
        if (outputPath.empty()) {
            problems.emplace_back("output video: is required");
        } else {
            try {
                subtitlePathFor(outputPath);
            } catch (const std::invalid_argument&) {
                problems.emplace_back("output video: must not end in .ass");
            }
        }
    }
    return problems;
}

bool RsansApp::runStep(Step step) {
    if (d_runner.isRunning() || !problemsFor(step).empty()) {
        return false;
    }

    const ProjectData project = d_form.project();
    const std::string ffmpeg = ffmpegPath;
    const std::string output = outputPath;
    const auto keepTokens = [this](ProjectData result) { d_form.setTokens(std::move(result.tokens)); };

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
    Component outputInput = TextInput(&outputPath, "path/to/video.mp4", singleLineInput());
    Component ffmpegInput = TextInput(&ffmpegPath, "ffmpeg", singleLineInput());

    struct StepButton {
        Step step;
        std::string label;
        Component button;
    };
    std::vector<StepButton> steps;
    for (const auto& [step, label] : std::vector<std::pair<Step, std::string>>{
             {Step::Analyze, "Analyze"}, {Step::Rhyme, "Detect rhymes"},
             {Step::Export, "Export"}, {Step::Full, "Full run"}}) {
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
        outputInput,
        ffmpegInput,
        Container::Horizontal(buttons),
    });

    return Renderer(container, [this, outputInput, ffmpegInput, steps, cancelButton] {
        // Buttons for steps that can't run are dimmed (pressing them does nothing)
        Elements buttonRow;
        Elements checks;
        for (const StepButton& entry : steps) {
            const std::vector<std::string> problems = problemsFor(entry.step);
            const bool canRun = problems.empty() && !d_runner.isRunning();
            buttonRow.push_back(canRun ? entry.button->Render() : entry.button->Render() | dim);

            Element status = problems.empty()
                ? text("ready") | color(ftxui::Color::Green)
                : text(problems.front() + (problems.size() > 1
                           ? "  (+" + std::to_string(problems.size() - 1) + " more)"
                           : "")) | color(ftxui::Color::Red);
            checks.push_back(hbox({text("  " + entry.label) | size(WIDTH, EQUAL, labelWidth), status}));
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
            hbox({text("  Output video") | size(WIDTH, EQUAL, labelWidth), outputInput->Render() | flex}),
            hbox({text("  FFmpeg") | size(WIDTH, EQUAL, labelWidth), ffmpegInput->Render() | flex}),
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

Component RsansApp::makeLyricsTab() const {
    return Renderer([this] {
        const ProjectData& project = d_form.project();
        if (project.tokens.empty()) {
            return vbox({text("No lyrics yet. Run Analyze on the Run tab.") | dim});
        }

        // One row per lyric line, words colored by their rhyme group
        std::map<int, Elements> lines;
        std::map<int, int> lineStartMs;
        for (const Token& token : project.tokens) {
            Element word = text(token.text);
            if (token.rhymeIndex.has_value() && !project.colorSwatch.empty()) {
                const std::size_t colorIndex =
                    static_cast<std::size_t>(token.rhymeIndex.value()) % project.colorSwatch.size();
                word = word | color(toColor(project.colorSwatch[colorIndex].hex())) | bold;
            }
            Elements& line = lines[token.lineIndex];
            if (!line.empty()) {
                line.push_back(text(" "));
            }
            line.push_back(word);
            if (lineStartMs.count(token.lineIndex) == 0) {
                lineStartMs[token.lineIndex] = token.startMs;
            }
        }

        Elements rows;
        for (auto& [lineIndex, words] : lines) {
            rows.push_back(hbox({text(formatTime(lineStartMs[lineIndex]) + "  ") | dim,
                                 hbox(std::move(words))}));
        }
        return vbox({
            text(std::to_string(project.tokens.size()) + " words. Colored words rhyme.") | dim,
            text(""),
            vbox(std::move(rows)) | yframe | vscroll_indicator | flex,
        });
    });
}

int runTui(const std::string& ffmpegPath) {
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
        app.scanFonts();  // In the background, so the UI appears straight away
        screen.Loop(app.component());
    }
    return 0;
}
