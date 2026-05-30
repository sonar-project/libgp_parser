#pragma once

#include "libgp_parser/error.hpp"
#include "libgp_parser/song.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace libgp_parser {
    /// Reads song metadata and track headers from GP3/GP4/GP5.
    /// @par TuxGuitar source
    /// common/TuxGuitar-gtp/.../GTPInputStream.java, readSong() (start: readInfo,
    /// tracks; measures/notes are still skipped here).
    /// @par Brief
    /// Detect version, parse info block and track headers.
    /// @par Visibility
    /// public (library API).
    [[nodiscard]] ParseResult<Song> load_gtp_song(const std::vector<std::uint8_t> &data);

    /// Returns true if bytes start with "FICHIER GUITAR PRO".
    /// @par TuxGuitar source GTPFileFormatDetector.
    /// @par Brief GTP file detection.
    /// @par Visibility public.
    [[nodiscard]] bool is_gtp_file(std::span<const std::uint8_t> data);
} // namespace libgp_parser
