<!-- Project Shields -->
[![Gtest][gtest-svg]][gtest-url]
![GitHub repo size][repo-size]

<!-- Readme Text -->
# RSANS Lyric Alignment & Visualization Engine

## Overview
RSANS is a headless, offline-first CLI tool that aligns audio with provided lyrics and generates time-synchronized lyric visualization videos. It uses offline speech recognition to extract word-level timing, maps that timing onto user-supplied lyrics, applies optional rhyme annotations, and renders karaoke-style highlights via FFmpeg. The system is file-driven and deterministic, designed for automation now and UI integration later.

## Features
- Offline audio processing (no cloud dependency)
- Plain-text lyrics input
- Word-level timing via offline speech recognition
- Deterministic alignment of audio timing to provided lyrics
- Optional rhyme-group annotations
- Karaoke-style word highlighting and color grouping
- Video export with burned-in visuals using FFmpeg
- File-driven CLI workflow with reproducible outputs
- JSON project state for caching and future UI integration


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
TBA
```

## Output Format
RSANS produces a rendered video file with time-synchronized lyric visualization burned into the frames.

Primary outputs:
- MP4 video containing the original audio and animated lyric overlays (karaoke-style word highlighting and optional rhyme-based color grouping)
- Intermediate artifacts (optional, kept in the project directory):
- JSON project state with tokenized lyrics, word timings, and rhyme group assignments
- ASS subtitle file used internally for precise timing and animation
- Logs describing alignment results and unmatched words

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

## Limitations
- Requires reasonably clean, clearly spoken or sung audio
- Lyrics must closely match the audio; large deviations reduce alignment quality
- Word-level timing accuracy depends on the speech recognition model
- Does not handle overlapping vocals or polyphonic speech
- Rhyme detection is user-defined; no automatic rhyme inference
- No real-time playback or editing (offline rendering only)
- Desktop UI not included (CLI-only)

<!-- CI Test Badges -->
[gtest-svg]:  https://github.com/dariustb/rsans-engine/actions/workflows/gtest.yml/badge.svg
[gtest-url]:  https://github.com/dariustb/rsans-engine/actions/workflows/gtest.yml
[repo-size]:  https://img.shields.io/github/repo-size/dariustb/rsans-engine
