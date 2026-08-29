#pragma once

#include "libgp_parser/error.hpp"
#include "libgp_parser/song.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace libgp_parser {
    /// Reads a GP3/GP4/GP5 song into the unified Song model.
    /// @par TuxGuitar source
    /// common/TuxGuitar-gtp GP3/GP4/GP5InputStream.readSong() plus
    /// GTPInputStream.read() which then runs GTPSongNormalizer.
    /// @par Brief
    /// Detect version, parse metadata, mixer, headers, measures, and notes.
    /// @par Visibility
    /// public (library API).
    [[nodiscard]] ParseResult<Song> load_gtp_song(const std::vector<std::uint8_t> &data);

    /// Returns true if bytes start with "FICHIER GUITAR PRO".
    /// @par TuxGuitar source GTPFileFormatDetector.
    /// @par Brief GTP file detection.
    /// @par Visibility public.
    [[nodiscard]] bool is_gtp_file(std::span<const std::uint8_t> data);

    /// Fixes percussion bank/program after a GTP import (GTPSongNormalizer).
    /// @par TuxGuitar source common/TuxGuitar-gtp/.../GTPSongNormalizer.java.
    /// @par Brief Sets bank 128 and GM channel 9 on percussion mixer rows.
    /// @par Visibility public.
    void normalize_gtp_channels(Song &song);
} // namespace libgp_parser
