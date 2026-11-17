<!-- Project Shields -->
![GitHub repo size][repo-size]
[![Gtest][gtest-svg]][gtest-url]
[![codecov][codecov-svg]][codecov-url]

<!-- Readme Text -->
# RSANS Lyric Alignment & Visualization Engine

## Overview
RSANS Engine is an offline-first CLI tool that turns a song into a time-synchronized lyric video. It transcribes the vocals with offline speech recognition ([whisper.cpp](https://github.com/ggml-org/whisper.cpp)) to get each word and its timing, groups rhyming words automatically using the [CMU Pronouncing Dictionary](https://github.com/cmusphinx/cmudict), and renders karaoke-style highlights with FFmpeg. The workflow is file-driven: each step reads and writes a JSON project file. An interactive terminal UI is in development.

## Features
- Offline audio processing (no cloud dependency)
- Accepts any audio format FFmpeg can read (MP3, FLAC, M4A, WAV at any sample rate, ...)
- Word-level timing via offline speech recognition
- Automatic rhyme detection, with rhyming words color-grouped
- Karaoke-style word highlighting
- Video export with burned-in visuals using FFmpeg
- Project checks before each step, listing every missing or invalid setting at once
- File-driven CLI workflow with JSON project state for caching and UI integration

## Requirements

### To build
- CMake 3.28 or newer
- A C++17 compiler (GCC or Clang) and a C compiler
- pkg-config, git, and clang-tidy (run on the project's own code during the build)
- libass and libsndfile development packages
- Internet access when configuring: whisper.cpp, CLI11, nlohmann/json, dr_libs, stb, CMUdict, FTXUI, and GoogleTest are downloaded by CMake

On Ubuntu/Debian:
```sh
sudo apt-get install build-essential cmake git pkg-config clang-tidy libass-dev libsndfile1-dev
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

Running `rsans` with no subcommand prints the help when used in a script or pipe. In a terminal it will open the interactive UI once that is built.

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
| `header.fontPath`, `layout.fontPath` | Required font files. Both fonts must be in the same folder |
| `audio.path` | Required. Any format FFmpeg can read |
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

Tests that run FFmpeg are skipped if `ffmpeg` is not on the `PATH`, and the export tests also skip if the DejaVu Sans font is not installed.

## Limitations
- English-only transcription
- Requires reasonably clean, clearly spoken or sung audio
- Lyrics come from the transcription, so misheard words have to be corrected in the project file
- Audio with no detectable words (e.g. instrumentals) is rejected
- Word-level timing accuracy depends on the speech recognition model
- Does not handle overlapping vocals or polyphonic speech
- No real-time playback or editing (offline rendering only)
- The header and layout fonts must be in the same folder
- Only one transcription can run at a time per process
- Interactive UI not yet available (CLI-only for now)

<!-- CI Test Badges -->
[repo-size]:   https://img.shields.io/github/repo-size/dariustb/rsans-engine
[gtest-svg]:   https://github.com/dariustb/rsans-engine/actions/workflows/gtest.yml/badge.svg
[gtest-url]:   https://github.com/dariustb/rsans-engine/actions/workflows/gtest.yml
[codecov-svg]: https://codecov.io/gh/dariustb/rsans-engine/graph/badge.svg
[codecov-url]: https://codecov.io/gh/dariustb/rsans-engine
