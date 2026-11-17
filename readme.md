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
- **Command line:** subcommands that read and write a JSON project file, for scripts and automation.

## Features
- Offline audio processing (no cloud dependency)
- Accepts WAV, FLAC, OGG, Opus, MP3 and AIFF audio at any sample rate (AAC/M4A is not supported)
- Word-level timing via offline speech recognition
- Automatic rhyme detection, with rhyming words color-grouped
- Karaoke-style word highlighting
- Video export with burned-in visuals using FFmpeg
- Project checks before each step, listing every missing or invalid setting at once
- Interactive terminal UI with live validation, progress, cancelling, a log, a file browser for every path, and a font picker for installed fonts
- File-driven CLI workflow with JSON project state for scripting

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
- **A Whisper model** in ggml format (`ggml-*.bin`). whisper.cpp's download script fetches one by size, e.g. `base.en`:
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

It opens on a start screen with the RSANS title and four options:
- **New project:** a blank project with default settings.
- **Load project:** choose a project `.json` file (the same format the command line uses) with the file browser. Its settings and any transcribed words are loaded; relative paths in it are read from the folder `rsans` was started in, as on the command line.
- **Credits:** the libraries RSANS is built on.
- **Quit**

The project editor has three tabs:
- **Project:** every setting, grouped into Song, Audio, Video, Lyrics, Analysis and Colors. Problems are checked as you type. A red `!` marks a field that needs attention: on its own it means the value is required, and any other problem (a missing file, a non-number) is written underneath. Choosing an audio file shows its length.
- **Run:** the output video path and FFmpeg path, then buttons for **Analyze**, **Detect rhymes**, **Export** and **Full run**. A step's button is dimmed until its settings are complete, and the Checks list shows what each step is still missing. While a step runs you get a progress bar, the current stage and a live log, and **Cancel** stops it (a cancelled export deletes its partial video).
- **Lyrics:** the transcribed words, one row per line with its start time, with rhyming words shown in their rhyme color.

Every file path can be typed, or picked with its **Browse** button (move right from the path, or Tab to it). The file browser starts in the folder of the current path and lists folders plus the file types that setting accepts (images for the cover, audio files, `.bin` models, and so on); tick **Show all files** to see everything. Type to filter the list, or type a path (`~` works) and press Enter to go straight to it. Enter opens a folder or chooses a file, Backspace with nothing typed goes up a folder, and Esc closes it. For the output video the typed text is the file name to save as, and `.mp4` is added if there's no extension.

Fonts are chosen from the fonts installed on your system. Press Enter on **Header font** or **Lyrics font** to open the picker, type part of a name to filter the list, and press Enter to choose; Esc closes it. Typing a path to a font file, or using **Browse files**, uses that file even if it isn't installed. Fonts are scanned in the background at startup, and the picker has a **Rescan fonts** button for fonts installed while it is running.

New projects start with 1920x1080 video on black, standard text sizes and six rhyme colors. When `rsans` is started from the repository folder, the bundled `data/cmudict.dict` is filled in too.

| Key | Action |
|---|---|
| Arrow keys / Tab | Move between tabs, fields and buttons |
| Enter | Press a button, open the font picker, or choose a font, file or folder |
| Backspace | In the file browser with nothing typed: go up a folder |
| Esc | Close the font picker or file browser |
| Ctrl+C | Quit (cancels a running step first) |

Projects can be loaded but not saved yet: quitting discards changes made in the UI.

### Command line
```sh
# Transcribe the audio into timed words
rsans analyze -i project.json -o out.json

# Detect rhymes in the transcribed words
rsans rhyme -i project.json -o out.json

# Export video based on project data
rsans export -i project.json -o out.mp4

# Full run of analyze -> rhyme -> export
rsans full -i project.json -o out.mp4

# Use a specific ffmpeg binary (works with any subcommand)
rsans --ffmpeg /opt/ffmpeg/bin/ffmpeg full -i project.json -o out.mp4
```

Running `rsans` with no subcommand prints the help when used in a script or pipe, instead of opening the interactive UI.

Each step checks that the project has what it needs before starting, and lists every problem if it doesn't:
```
Error: Project is not ready to run:
  model.path: file not found: models/ggml-base.en.bin
  cmudict: is required
```

## Project File
A project is a JSON file. A minimal one, before `analyze` has been run:
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
| `header.fontPath`, `layout.fontPath` | Required font files. Fonts installed on the system are found by family name; a font that isn't installed must be in the same folder as the other font |
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
- JSON project state from `analyze` and `rhyme`, with the words, their timings, and rhyme group assignments

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
- Lyrics come from the transcription, so misheard words have to be corrected in the project file (the UI can't edit them yet)
- Audio with no detectable words (e.g. instrumentals) is rejected
- Word-level timing accuracy depends on the speech recognition model
- Does not handle overlapping vocals or polyphonic speech
- No real-time playback or editing (offline rendering only)
- Fonts render in their regular style only (no bold or italic)
- A font that isn't installed on the system must be in the same folder as the other font
- Only one transcription can run at a time per process
- The interactive UI can load project files but not save them yet, and the Whisper model is chosen by file path

<!-- CI Test Badges -->
[repo-size]:   https://img.shields.io/github/repo-size/dariustb/rsans-engine
[gtest-svg]:   https://github.com/dariustb/rsans-engine/actions/workflows/gtest.yml/badge.svg
[gtest-url]:   https://github.com/dariustb/rsans-engine/actions/workflows/gtest.yml
[codecov-svg]: https://codecov.io/gh/dariustb/rsans-engine/graph/badge.svg
[codecov-url]: https://codecov.io/gh/dariustb/rsans-engine
