<!-- Project Shields -->
![GitHub repo size][repo-size]
[![Gtest][gtest-svg]][gtest-url]
[![codecov][codecov-svg]][codecov-url]

<!-- Readme Text -->
# RSANS Lyric Alignment & Visualization Engine

## Overview
RSANS Engine is an offline-first tool that turns a song into a time-synchronized lyric video. It transcribes the vocals with offline speech recognition ([whisper.cpp](https://github.com/ggml-org/whisper.cpp)) to get each word and its timing, groups rhyming words automatically using the [CMU Pronouncing Dictionary](https://github.com/cmusphinx/cmudict), and renders karaoke-style highlights with FFmpeg.

It can be used two ways:
- **Interactive terminal UI:** run `rsans` in a terminal to fill in the settings, run each step with live progress, and preview the lyrics.
- **Command line:** subcommands that read and write a project file (`.rsans`, JSON inside), for scripts and automation.

## Features
- Offline audio processing (no cloud dependency)
- Accepts WAV, FLAC, OGG, Opus, MP3 and AIFF audio at any sample rate (AAC/M4A is not supported)
- Word-level timing via offline speech recognition
- Automatic rhyme detection, with rhyming words color-grouped
- Karaoke-style word highlighting
- Video export with burned-in visuals using FFmpeg
- Project checks before each step, listing every missing or invalid setting at once
- Interactive terminal UI: a start screen, live validation as you type, progress with cancelling and a log, and a lyrics view for correcting misheard words and rhyme groups
- Pickers for installed fonts and for Whisper models, with downloads of missing models; a file browser for every path
- Projects saved and loaded as `.rsans` files, with unsaved-changes prompts
- File-driven CLI workflow using the same project files, for scripting

## Requirements

### To build
- CMake 3.28 or newer
- A C++17 compiler (GCC or Clang) and a C compiler
- pkg-config, git, and clang-tidy (run on the project's own code during the build)
- libass, libsndfile and fontconfig development packages
- Internet access when configuring: whisper.cpp, CLI11, nlohmann/json, dr_libs, stb, CMUdict, FTXUI, and GoogleTest are downloaded by CMake

On Ubuntu/Debian:
```sh
sudo apt-get install build-essential cmake git pkg-config clang-tidy libass-dev libsndfile1-dev libfontconfig-dev
```

### To run
- **FFmpeg** on your `PATH` (or pass `--ffmpeg /path/to/ffmpeg`). Used to convert audio for transcription and to render the video.
- **A Whisper model** in ggml format (`ggml-*.bin`). The interactive UI lists whisper.cpp's models and can download them for you (see below). Or fetch one with whisper.cpp's download script, e.g. `base.en`:
  ```sh
  sh build/_deps/whisper-src/models/download-ggml-model.sh base.en
  ```
- **The CMU Pronouncing Dictionary**, copied to `data/cmudict.dict` by the build.

Linux and macOS are supported. Windows is not.

## Installation
Build:
```sh
cmake -S . -B build
cmake --build build
```

Install rsans to ~/.local/bin (make sure this is on your PATH):
```sh
cmake --install build --prefix ~/.local
```

## How to Use

### Interactive UI
Run `rsans` with no arguments in a terminal:
```sh
rsans
rsans --ffmpeg /opt/ffmpeg/bin/ffmpeg   # with a specific ffmpeg binary
```

#### Start screen
It opens on a start screen with the RSANS title and four options:
- **New project:** a blank project with default settings: 1920x1080 video on black, standard text sizes and six rhyme colors. When `rsans` is started from the repository folder, the bundled `data/cmudict.dict` is filled in too.
- **Load project:** choose a project file (`.rsans`, or an older `.json` one) with the file browser. Its settings and any transcribed words are loaded; relative paths in it are read from the folder `rsans` was started in, as on the command line.
- **Credits:** the libraries RSANS is built on.
- **Quit**

#### Editor
The editor has three tabs, with the project's file name and **Save**, **Save as** and **Menu** buttons at the top right:
- **Project:** every setting, grouped into Song, Audio, Video, Lyrics, Analysis and Colors. Problems are checked as you type. A red `!` marks a field that needs attention: on its own it means the value is required, and any other problem (a missing file, a non-number) is written underneath. Choosing an audio file shows its length.
- **Run:** the app settings (FFmpeg and the whisper.cpp folder, see [Settings](#settings)), then buttons for **Analyze**, **Detect rhymes**, **Export...** and **Full run...**. Export and Full run first ask where to save the video, every time: the save dialog suggests the project's name with `.mp4` (e.g. `night-drive.mp4` for `night-drive.rsans`, or the song title before the project is saved), in the project's folder, or the folder you last saved this project's video to. `.mp4` is added if you leave out the extension, and replacing an existing video needs a second Enter. The subtitles are saved beside it as `.ass`. A step's button is dimmed until its settings are complete, and the Checks list shows what each step is still missing. While a step runs you get a progress bar, the current stage and a live log, and **Cancel** stops it (a cancelled export deletes its partial video).
- **Lyrics:** the transcribed words, one row per line with its start time, with rhyming words shown in their rhyme color. Misheard words can be corrected here: move to a word with the arrow keys (the bar at the bottom shows it and when it's sung) and press Enter to edit it, or to **Delete word** for one that was never sung. Its timing is kept. Rhyme colors aren't updated by changing a word, so the tab reminds you to run **Detect rhymes** again. The same dialog sets the word's **Rhyme group** by hand: **None**, any group in use (shown in its color, with the words already in it), or **New group**. Running Detect rhymes again replaces groups set by hand. Editing waits while a step is running, since the step replaces the words when it finishes.

**Menu** (or Esc) goes back to the start screen. It waits while a step is running, since the step's results belong to the open project.

Pickers and the file browser open on top of the editor, with a yellow frame.

#### Saving
Press **Ctrl+S**, or **Save**, to save the project, including the transcribed words, as a `.rsans` file (JSON inside, the same format the command line uses). Projects can be saved at any point, even with settings still missing.
- A new project asks where to save it first, suggesting a name from the song title. After that, Ctrl+S saves straight to the same file.
- **Save as** saves a copy somewhere else. The name always gets the `.rsans` extension (`song.json` becomes `song.rsans`), and it asks before replacing a different file.
- A project loaded from an older `.json` file saves back to that file; use **Save as** to switch it to `.rsans`.
- The file name at the top shows `*` while there are unsaved changes. Quitting or going back to the start screen with unsaved changes asks whether to save first.

#### Choosing files
Every file path can be typed, or picked with its **Browse** button (move right from the path, or Tab to it). The file browser starts in the folder of the current path and lists folders plus the file types that setting accepts (images for the cover, audio files, and so on); tick **Show all files** to see everything. Type to filter the list, or type a path (`~` works) and press Enter to go straight to it. Enter opens a folder or chooses a file, Backspace with nothing typed goes up a folder, and Esc closes it. When saving (a project or a video) the typed text is the file name to save as.

#### Fonts
Fonts are chosen from the fonts installed on your system. Press Enter on **Header font** or **Lyrics font** to open the picker, type part of a name to filter the list, and press Enter to choose. Typing a path to a font file, or using **Browse files**, uses that file even if it isn't installed. Fonts are scanned in the background at startup, and the picker has a **Rescan fonts** button for fonts installed while it is running.

Font files can be anywhere, installed or not. When a video is exported, the two font files are copied into a temporary folder for that render and deleted when it ends, so nothing is copied until you export. The subtitles find a font by the name inside it, so if a font's name doesn't match its file (for example a name edited in the project file), the font row shows what the file's font is actually called, and the Run tab marks Export as "ready, but..." until you choose the font again.

#### Whisper models
Press Enter on **Whisper model** to choose from the models whisper.cpp offers. The list shows one model per size: tiny.en, base.en, small.en, medium.en, large-v3 and large-v3-turbo (RSANS transcribes English, so the English-only versions are offered), plus any you've already installed. Tick **Show all** for every variant, including the multilingual and quantized ones. **Other file...** picks any `.bin` model instead.

Installed models are marked ✓ and the rest show their download size, e.g. `medium.en (1.5 GiB)`; a `~` means the size is estimated. Choosing a model that isn't installed asks first, then downloads it with whisper.cpp's own `download-ggml-model.sh` (using curl or wget). The download runs on the Run tab with progress and can be cancelled; a failed or cancelled download leaves no partial file.

The models and their sizes are read from a whisper.cpp checkout (its `models/download-ggml-model.sh` and `models/README.md`), and models are stored in its `models/` folder, where that script puts them. By default that's the copy CMake downloads to build RSANS (`build/_deps/whisper-src`). To use your own checkout, set it when configuring, or per run with an environment variable:
```sh
cmake -S . -B build -DRSANS_WHISPER_CPP_DIR=~/whisper.cpp
WHISPER_CPP_DIR=~/whisper.cpp rsans
```
or type the folder into **whisper.cpp** on the Run tab, which is remembered (see below).

#### Settings
The FFmpeg path and whisper.cpp folder belong to your computer rather than a project, so they're on the Run tab instead of in the project file. Changing one there saves it to `~/.config/rsans/settings.json` (or `$XDG_CONFIG_HOME/rsans/settings.json`), and it's used every time RSANS starts. Only the settings you change are saved. `--ffmpeg` and `WHISPER_CPP_DIR` still win for the run they're given in, and aren't saved unless you then change that setting on the Run tab. A missing or damaged settings file just means the defaults are used.

#### Keys
| Key | Action |
|---|---|
| Arrow keys / Tab | Move between tabs, fields and buttons |
| Enter | Press a button, open a picker, choose a font, model, file or folder, or edit the highlighted word |
| Backspace | In the file browser with nothing typed: go up a folder |
| Esc | Close the open dialog; in the editor with none open, go back to the start screen |
| Ctrl+S | Save the project |
| Ctrl+C | Quit (cancels a running step first; asks to save unsaved changes) |

### Command line
```sh
# Transcribe the audio into timed words
rsans analyze -i project.rsans -o out.rsans

# Detect rhymes in the transcribed words
rsans rhyme -i project.rsans -o out.rsans

# Export video based on project data
rsans export -i project.rsans -o out.mp4

# Full run of analyze -> rhyme -> export
rsans full -i project.rsans -o out.mp4

# Use a specific ffmpeg binary (works with any subcommand)
rsans --ffmpeg /opt/ffmpeg/bin/ffmpeg full -i project.rsans -o out.mp4
```

Running `rsans` with no subcommand prints the help when used in a script or pipe, instead of opening the interactive UI.

Each step checks that the project has what it needs before starting, and lists every problem if it doesn't:
```
Error: Project is not ready to run:
  model.path: file not found: models/ggml-base.en.bin
  cmudict: is required
```

## Project File
A project is a `.rsans` file containing JSON. The interactive UI saves projects with this extension; the command line reads and writes any file name (older `.json` projects work everywhere). A minimal one, before `analyze` has been run:
```json
{
    "header": {
        "title": "Song Title", "titleSize": 64,
        "artist": "Artist Name", "artistSize": 36,
        "fontName": "DejaVu Sans", "fontPath": "fonts/DejaVuSans.ttf",
        "media": "cover.png"
    },
    "audio": { "path": "song.mp3", "length": null },
    "video": { "width": 1920, "height": 1080, "background": "black" },
    "layout": {
        "fontName": "DejaVu Sans", "fontPath": "fonts/DejaVuSans.ttf",
        "fontSize": 48, "lineHeight": 60
    },
    "model": { "path": "models/ggml-base.en.bin" },
    "cmudict": "data/cmudict.dict",
    "colorSwatch": ["#FF8800", "#00AAFF", "#AA55FF"],
    "tokens": []
}
```

| Field | Notes |
|---|---|
| `header.title`, `header.artist` | Required, shown above the lyrics |
| `header.media` | Required cover image: PNG, JPEG, BMP, GIF or TGA (not video) |
| `header.fontName`, `layout.fontName` | Required font family names, as the font itself names them (e.g. `DejaVu Sans`) |
| `header.fontPath`, `layout.fontPath` | Required font files, installed or not, in any folder |
| `audio.path` | Required. WAV, FLAC, OGG, Opus, MP3 or AIFF (the formats libsndfile reads). AAC/M4A is not supported; convert it first, e.g. `ffmpeg -i song.m4a song.flac` |
| `audio.length` | Seconds. Leave `null` to have it measured from the audio |
| `video.width`, `video.height` | Required, positive, and even (required by the H.264 encoder) |
| `video.background` | Required. An FFmpeg color name or hex value |
| `model.path` | Whisper model, required by `analyze` |
| `cmudict` | Pronouncing dictionary, required by `rhyme` |
| `colorSwatch` | `#RRGGBB` colors for rhyme groups; at least one is needed once there are rhymes |
| `tokens` | The timed words. Filled in by `analyze`, rhyme groups added by `rhyme` |

What each step needs:
- **analyze:** `audio.path`, `model.path`
- **rhyme:** `cmudict`, and tokens from `analyze`
- **export:** the header, audio, video and layout settings, and tokens from `analyze`
- **full:** everything above, plus at least one color in `colorSwatch`

## Output Format
RSANS produces a rendered video file with time-synchronized lyric visualization burned into the frames.

Primary outputs:
- MP4 video containing the original audio and animated lyric overlays (karaoke-style word highlighting and rhyme-based color grouping)
- The ASS subtitle file used to render the lyrics, written next to the video with the same name (`out.mp4` → `out.ass`). Useful for checking timings.

Intermediate artifacts:
- Project files (`.rsans`, JSON) from `analyze` and `rhyme`, with the words, their timings, and rhyme group assignments

## Testing
All tests are built as part of the normal build

Run the full test suite:
```sh
cd build
ctest --output-on-failure
```

(Optional) Run the test binary directly:
```sh
cd build/tests
./test_rsans
```

Tests that run FFmpeg are skipped if `ffmpeg` is not on the `PATH`. The export tests and some font tests also skip if the DejaVu Sans font is not installed.

## Limitations
- English-only transcription
- Requires reasonably clean, clearly spoken or sung audio
- Lyrics come from the transcription. Misheard words can be corrected or removed, and rhyme groups changed, on the Lyrics tab, but words can't be added, split, merged or retimed
- Audio with no detectable words (e.g. instrumentals) is rejected
- Word-level timing accuracy depends on the speech recognition model
- Does not handle overlapping vocals or polyphonic speech
- No real-time playback or editing (offline rendering only)
- Fonts render in their regular style only (no bold or italic)
- Only one transcription can run at a time per process
- Downloading models from the UI needs internet access, `sh`, and curl or wget; everything else runs offline

<!-- CI Test Badges -->
[repo-size]:   https://img.shields.io/github/repo-size/dariustb/rsans-engine
[gtest-svg]:   https://github.com/dariustb/rsans-engine/actions/workflows/gtest.yml/badge.svg
[gtest-url]:   https://github.com/dariustb/rsans-engine/actions/workflows/gtest.yml
[codecov-svg]: https://codecov.io/gh/dariustb/rsans-engine/graph/badge.svg
[codecov-url]: https://codecov.io/gh/dariustb/rsans-engine
