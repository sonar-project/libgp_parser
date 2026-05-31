# libgp_parser

[![Test libgp_parser](https://github.com/sonar-project/libgp_parser/actions/workflows/ci-tests.yaml/badge.svg)](https://github.com/sonar-project/libgp_parser/actions/workflows/ci-tests.yaml)

A modern C++23 library for parsing Guitar Pro files. Incrementally ported from TuxGuitar, this project focuses on high robustness, strict type safety, and an idiomatic modern C++ API. Contributions and feedback are welcome!

This implementation was developed as a clean-room port of the Guitar Pro file parsing logic. It utilizes C++23 features to provide a type-safe and robust API. The parser architecture was designed independently from legacy Java-based implementations to leverage modern C++ memory safety and performance patterns.

It is being incrementally ported from [TuxGuitar](https://github.com/helge17/tuxguitar)’s Java GPX module of TuxGuitar, with a clear focus on robustness, type safety, and modern C++ API design.

## Motivation

This project arose from the need to process Guitar Pro metadata efficiently and modularly within
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
| `tests/` | Catch2 unit tests (TDD) |
| `example/` | `gp_info` CLI — prints metadata and track count |

## Build & Integration

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/example/gp_info path/to/file.gpx
```

Options: `-DLIBGP_PARSER_BUILD_TESTS=OFF`, `-DLIBGP_PARSER_BUILD_EXAMPLE=OFF`.

## Domain model (phase 1)

* `Song` / `SongMetadata` — mirrors `GPXScore` + track list (`GPXDocument`)
* `Track` — mirrors `GPXTrack` (tuning, GM channels, capo, color)

Dependencies (CMake FetchContent): **pugixml**, **miniz**, **Catch2** (tests only).

* `parse_gpx_score_metadata()` — score.gpif XML (`GPXDocumentReader.readScore`)
* `GpxZipArchive` — ZIP extraction for `.gpx` (GP7: `Content/score.gpif`)
* `load_song(path)` — auto-detects format and fills metadata + track count where available

| Extension | Format | Status |
| ----------- | -------- | -------- |
| `.gp` | GP7 (ZIP) | metadata |
| `.gpx` | GP6 (BCFS/BCFZ) | metadata |
| `.gp3`–`.gp5` | GTP binary | metadata + track count |
| GP6 BCFZ custom container | | metadata |

## Current Status & Roadmap

The current state of the library meets the requirements for my main project, **sonarpractice**. The primary focus lies on the efficient extraction of metadata and the structural parsing of Guitar Pro files.

**Current Scope:** Metadata parsing and the basic track structure function reliably within my application environment.

**Future Plans:** The full implementation of track and note parsing (GPX/GTP) is planned as a future extension.
  
**Call for Contributions:** As I am currently focusing primarily on the further development of the core   **sonarpractice** application, contributions to extend the parser logic—particularly regarding note and track data—are highly welcome. If you would like to contribute features, I would be delighted to receive a pull request!

## Workflow

1. Analysis: Understanding the reference implementation (Java/TuxGuitar).
2. Testing: Writing a Catch2 test for the specific behavior.
3. Implementation: Minimal C++ code, adhering to clang-tidy guidelines.
4. Refactoring: Optimization using references and Modern C++ features.

## Usage

The library is designed to be simple and safe to use. You can find a minimal example for extracting song metadata in the `example/` folder.

A brief overview of the API:

```cpp
#include <libgp_parser/load_song.hpp>
#include <iostream>

int main() {    
    // Automatically loads the file and detects the format (GP,GPX,GP3-GP5)
    const auto result = libgp_parser::load_song("path_to_guitarpro_file");
    
    if (!result) {
        std::cerr << "Error: " << result.error().message << '\n';
        return 1;
    }

    const libgp_parser::Song &song = result.value();
    
    std::cout << "Title:  " << song.name() << '\n';
    std::cout << "Artist: " << song.artist() << '\n';
    std::cout << "Album:  " << song.album() << '\n';
    std::cout << "Author: " << song.author() << '\n';

    return 0;
}
```

## License

![License: AGPL v3](https://img.shields.io/badge/License-AGPLv3-blue.svg)

This project is licensed under the GNU Affero General Public License v3.0 (AGPL-3.0).
