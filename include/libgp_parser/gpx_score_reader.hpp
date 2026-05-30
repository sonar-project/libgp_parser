#pragma once

#include "libgp_parser/error.hpp"
#include "libgp_parser/song.hpp"

#include <string_view>

namespace libgp_parser {

    /// Reads fields from the Score XML element in score.gpif.
    /// @par TuxGuitar source
    /// common/TuxGuitar-gpx/.../GPXDocumentReader.java, readScore().
    /// @par Brief
    /// Fills title, subtitle, artist, and other score text fields.
    /// @par Visibility
    /// public (library API).
    [[nodiscard]] ParseResult<SongMetadata> parse_gpx_score_metadata(std::string_view xml);

} // namespace libgp_parser
