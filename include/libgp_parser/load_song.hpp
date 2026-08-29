#pragma once

#include "libgp_parser/error.hpp"
#include "libgp_parser/song.hpp"

#include <filesystem>

namespace libgp_parser {

    /// Loads a Guitar Pro file and builds a Song (TuxGuitar import mapping).
    /// @par TuxGuitar source
    /// GPXInputStream / GTPInputStream → GPXDocumentReader + GPXDocumentParser
    /// or GP3/4/5InputStream.readSong() plus GTPSongNormalizer.
    /// @par Brief
    /// Detects GPX (ZIP/BCFS) or GTP and fills metadata, tracks, measures, and notes.
    /// @par Visibility
    /// public (library API).
    [[nodiscard]] ParseResult<Song> load_song(const std::filesystem::path &path);

} // namespace libgp_parser
