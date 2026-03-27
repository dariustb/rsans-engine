<!-- Project Shields -->
![GitHub repo size][repo-size]
[![Gtest][gtest-svg]][gtest-url]
[![codecov][codecov-svg]][codecov-url]

<!-- Readme Text -->
# RSANS Lyric Alignment & Visualization Engine

## Overview
RSANS Engine is a headless, offline-first CLI tool that generates time-synchronized lyric visualization videos. It tokenizes a plain-text lyrics file, uses offline speech recognition to obtain word-level timestamps, optionally annotates rhyme groups, and renders karaoke-style highlights via FFmpeg. The system is file-driven and deterministic, designed for automation and future UI integration.

## Features
- Plain-text lyrics file as the authoritative word source
- Word-level timestamps via Whisper (offline, no cloud dependency)
- Automatic fallback to Whisper transcription when no lyrics file is provided
- Deterministic alignment of audio timing to provided lyrics
- Optional rhyme-group annotations via CMU Pronouncing Dictionary
- Karaoke-style word highlighting with rhyme-based color grouping
- Video export with burned-in visuals using FFmpeg
- JSON project state for caching and pipeline inspection
- File-driven CLI workflow with reproducible outputs

## How It Works

The pipeline has three stages, each producing or consuming a JSON project file:

```
project.json
     │
     ▼
  analyze          Parse lyrics.txt → tokenize words → run Whisper for
                   timestamps → align Whisper output back onto lyric
                   tokens → write token list with timings to output JSON.
     │
     ▼
   rhyme           Look up each token in CMUDict → detect rhyme groups via
                   phoneme matching → annotate tokens with a rhymeIndex.
     │
     ▼
  export           Render an ASS subtitle file from the timed, annotated
                   tokens → pass audio + subtitle to FFmpeg → output MP4.
```

### Lyrics-guided alignment (default)
When `audio.lyricsPath` is set in the project JSON, the lyrics file drives tokenization:
1. The lyrics file is parsed into lines and word tokens, preserving original text for display and producing normalized forms for matching.
2. Whisper receives the full lyrics text as an `initial_prompt` to guide transcription, then runs forced alignment on the audio.
3. Each lyric token is matched to the nearest Whisper token by normalized text (greedy forward scan). Tokens Whisper missed are assigned interpolated timestamps from their matched neighbours.

### Whisper fallback
When `audio.lyricsPath` is absent or empty, Whisper transcribes the audio freely and its output is used directly as the token source. This is useful for spoken-word or ad-lib content without a lyrics file.

## Project File

All pipeline configuration lives in a single JSON file:

```json
{
  "header": {
    "fontName": "Quattrocento",
    "fontPath": "fonts/vid_fonts/Quattrocento-Bold.ttf",
    "title":    "Song Title",
    "titleSize": 72,
    "artist":   "Artist Name",
    "artistSize": 40,
    "media":    "cover.jpg"
  },
  "audio": {
    "path":       "songs/track.wav",
    "lyricsPath": "lyrics.txt",
    "length":     null
  },
  "video": {
    "width":      1080,
    "height":     1920,
    "background": "#E6E5D8"
  },
  "layout": {
    "fontName":   "Quattrocento Sans",
    "fontPath":   "fonts/vid_fonts/QuattrocentoSans-Regular.ttf",
    "fontSize":   36,
    "lineHeight": 74
  },
  "model": {
    "path": "/path/to/ggml-base.en.bin"
  },
  "cmudict":     "data/cmudict.dict",
  "tokens":      [],
  "colorSwatch": ["#FFF4A3", "#FFD6C9", "..."]
}
```

`audio.lyricsPath` is optional. Remove it (or set it to `null`) to use Whisper as the sole transcription and timing source.

`audio.length` can be set to a number (seconds) or `null` to have it derived automatically from the audio file.

## Installation
Build:
```sh
cmake -S . -B build
cmake --build build
```

Install `rsans` to `~/.local/bin` (ensure this is on your `PATH`):
```sh
cmake --install build --prefix ~/.local
```

## Usage
```sh
# Analyze audio — tokenize lyrics and assign word-level timestamps
rsans analyze -i project.json -o analyzed.json

# Detect rhymes — annotate tokens with rhyme group indices
rsans rhyme -i analyzed.json -o rhymed.json

# Export video — render MP4 with karaoke-style lyric overlay
rsans export -i rhymed.json -o out.mp4

# Full pipeline — analyze → rhyme → export in one step
rsans full -i project.json -o out.mp4
```

Intermediate JSON files can be inspected or edited between stages. Re-run any stage independently without re-processing the others.

## Output
- **MP4** — video with original audio and animated lyric overlay; rhyming words share a highlight color drawn from `colorSwatch`
- **JSON** — project state after each stage, containing tokenized lyrics, word timings, and rhyme annotations
- **ASS** — subtitle file generated internally for FFmpeg; kept in the project directory for inspection

## Testing
Tests are built as part of the normal build.

Run the full test suite:
```sh
cd build && ctest --output-on-failure
```

Run the test binary directly:
```sh
./build/tests/test_rsans
```

## Limitations
- Audio must be English
- Audio must be 16 kHz WAV (resampling not yet supported)
- Alignment quality degrades when the lyrics file significantly diverges from what is sung
- Whisper timestamp accuracy depends on the model size; larger models improve alignment
- Overlapping vocals and polyphonic speech are not handled
- Rhyme detection uses the CMU Pronouncing Dictionary; words absent from the dictionary are silently skipped
- CLI only — no real-time playback or interactive editing
- All fonts used in a single project must share the same parent directory

<!-- CI Test Badges -->
[repo-size]:   https://img.shields.io/github/repo-size/dariustb/rsans-engine
[gtest-svg]:   https://github.com/dariustb/rsans-engine/actions/workflows/gtest.yml/badge.svg
[gtest-url]:   https://github.com/dariustb/rsans-engine/actions/workflows/gtest.yml
[codecov-svg]: https://codecov.io/gh/dariustb/rsans-engine/graph/badge.svg
[codecov-url]: https://codecov.io/gh/dariustb/rsans-engine
