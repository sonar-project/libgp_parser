#pragma once

#include "libgp_parser/error.hpp"
#include "libgp_parser/song.hpp"

#include <filesystem>

namespace libgp_parser {

    /// Loads a Guitar Pro file and builds a Song with metadata and tracks.
    /// @par TuxGuitar source
    /// Same flow as GPXInputStream / GTPInputStream → GPXDocumentReader.read() or
    /// readSong(), without a full TGSong (phase-1 fields only).
    /// @par Brief
    /// Detects GPX (ZIP/BCFS) or GTP and fills Song including tuning.
    /// @par Visibility
    /// public (library API).
    [[nodiscard]] ParseResult<Song> load_song(const std::filesystem::path &path);

} // namespace libgp_parser
