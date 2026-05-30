#pragma once

#include "libgp_parser/error.hpp"
#include "libgp_parser/track.hpp"

#include <string_view>
#include <vector>

namespace libgp_parser {

    /// Reads all Track elements under Tracks in score.gpif.
    /// @par TuxGuitar source
    /// common/TuxGuitar-gpx/.../GPXDocumentReader.java, readTracks().
    /// @param is_gp7 true for GP7 (.gp ZIP), false for GP6 (.gpx).
    /// @par Brief
    /// Name, tuning, MIDI channels, and color per track.
    /// @par Visibility
    /// public (library API).
    [[nodiscard]] ParseResult<std::vector<Track>> parse_gpx_tracks(std::string_view xml,
                                                                   bool is_gp7);

} // namespace libgp_parser
