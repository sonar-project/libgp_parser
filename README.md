# libgp_parser

[![Test libgp_parser](https://github.com/sonar-project/libgp_parser/actions/workflows/ci-tests.yaml/badge.svg)](https://github.com/sonar-project/libgp_parser/actions/workflows/ci-tests.yaml)

A modern C++23 library for parsing Guitar Pro files. Incrementally ported from TuxGuitar, this project focuses on high robustness, strict type safety, and an idiomatic modern C++ API. Contributions and feedback are welcome!

This implementation was developed as a clean-room port of the Guitar Pro file parsing logic. It utilizes C++23 features to provide a type-safe and robust API. The parser architecture was designed independently from legacy Java-based implementations to leverage modern C++ memory safety and performance patterns.

It is being incrementally ported from [TuxGuitar](https://github.com/helge17/tuxguitar)’s Java GPX and GTP modules, with a clear focus on robustness, type safety, and modern C++ API design.

## Motivation

This project arose from the need to process Guitar Pro files efficiently and modularly within
custom C++ applications, without having to integrate the parser directly into the application code.

Currently, it serves as the foundation for the redevelopment of sonarpractice, with a focus on
state-of-the-art standards (C++23), Test-Driven Development (TDD), and a clean architecture.

## Philosophy & Code Quality

### Quality Assurance

* Formatting:
  * Consistent code via .clang-format (LLVM-based).

* Safety First:
  * The library consistently prioritizes references over pointers to minimize null-pointer issues.

* Automated Analysis:
  * We rely on clang-tidy with a restrictive rule set to eliminate security and performance risks early on.

## Layout

| Path | Purpose |
| ------ | --------- |
| `include/libgp_parser/` | Public headers |
| `src/` | Parser implementation |
| `test/` | Catch2 unit tests (TDD) |
| `example/` | `gp_info` (metadata) and `gp_score` (measures, notes, effects) |

## Build & Integration

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/example/gp_info path/to/file.gpx
./build/example/gp_score path/to/file.gp5
```

Options: `-DLIBGP_PARSER_BUILD_TESTS=OFF`, `-DLIBGP_PARSER_BUILD_EXAMPLE=OFF`.

FetchContent consumers should use tag **v0.2.0** (or later). The CMake target is `libgp_parser::libgp_parser`.

Dependencies (CMake FetchContent): **pugixml**, **miniz**, **OpenSSL** (`libcrypto`, edit-locked `.gp`), **Catch2** (tests only).

## Domain model

`load_song(path)` is the consumer entry point. It auto-detects GTP or GPX and fills a `Song` using TuxGuitar’s import mapping (not a lossless GP clone).

* `Song` / `SongMetadata` — title, artist, album, tempo, measure headers, mixer channels, tracks
* `Track` — name, tuning, GM program/channels, capo, lyrics (GTP), measures
* `MeasureHeader` — time signature, tempo, repeats, marker, triplet feel
* `Measure` / `Beat` / `Voice` — up to two voices, chords, strokes, beat text
* `Note` / `NoteEffect` — fret, string, velocity, ties; bend, harmonic, grace, trill, tremolo, slides, etc.
* `Channel`, `Lyric`, `Chord`, `Marker`

Lower-level helpers (`parse_gpx_score_metadata`, `parse_gpx_tracks`, `parse_gpx_initial_tempo`) remain for tests; applications should call `load_song()`.

| Extension | Format | Status |
| ----------- | -------- | -------- |
| `.gp` | GP7/GP8 (ZIP, `VERSION` 7.x/8.x; edit-locked `score.gpif` decrypted) | full import |
| `.gpx` | GP6 (BCFS/BCFZ) | full import |
| `.gp3`–`.gp5` | GTP binary | full import |
| GP6 BCFZ custom container | | full import |

**Mapping limits** (same as TuxGuitar): mix-change applies **tempo only**; GPX lyrics are not imported; GTP keeps one lyric track; max two voices; slide is a boolean; no export; no GP1/GP2.

Playback helpers in `timeline.hpp`: MIDI pitch, tick-to-milliseconds, and repeat expansion (`MidiRepeatController`).

## Current Status & Roadmap

Import parity with TuxGuitar’s GP3–GP5 and GP6/GP7 readers is in place: metadata, tracks, measures, notes, effects, chords, GTP lyrics, and mixer channels.

**Current Scope:** `load_song()` returns a complete `Song` graph suitable for cataloguing and in-app notation/playback.

**Future Plans:** finer `preciseStart` timing if tuplets need it; export is out of scope.

**Call for Contributions:** Bug reports against real Guitar Pro files and extra fixtures (repeats, mix-change, two voices) are especially welcome.

## Workflow

1. Analysis: Understanding the reference implementation (Java/TuxGuitar).
2. Testing: Writing a Catch2 test for the specific behavior.
3. Implementation: Minimal C++ code, adhering to clang-tidy guidelines.
4. Refactoring: Optimization using references and Modern C++ features.

## Usage

The library is designed to be simple and safe to use. `example/main.cpp` (`gp_info`) prints metadata; `example/gp_score.cpp` dumps the score.

```cpp
#include <libgp_parser/load_song.hpp>
#include <iostream>

int main() {
    const auto result = libgp_parser::load_song("path_to_guitarpro_file");

    if (!result) {
        std::cerr << "Error: " << result.error().message << '\n';
        return 1;
    }

    const libgp_parser::Song &song = result.value();

    std::cout << "Title:  " << song.name() << '\n';
    std::cout << "Artist: " << song.artist() << '\n';
    std::cout << "Album:  " << song.album() << '\n';
    std::cout << "Tempo:  " << song.tempo_bpm << '\n';

    for (const libgp_parser::Track &track : song.tracks) {
        for (const libgp_parser::Measure &measure : track.measures) {
            const libgp_parser::MeasureHeader &header = song.header_for(measure);
            for (const libgp_parser::Beat &beat : measure.beats) {
                for (const libgp_parser::Note &note : beat.voice(0).notes) {
                    (void)header;
                    (void)note;
                }
            }
        }
    }
}
```

## License

![License: AGPL v3](https://img.shields.io/badge/License-AGPLv3-blue.svg)

This project is licensed under the GNU Affero General Public License v3.0 (AGPL-3.0).
