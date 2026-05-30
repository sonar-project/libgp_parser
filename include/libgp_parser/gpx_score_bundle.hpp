#pragma once

#include "libgp_parser/error.hpp"

#include <cstdint>
#include <vector>

namespace libgp_parser {

    /// Extracts score.gpif bytes from a GP7 ZIP or GP6 BCFS/BCFZ container.
    /// @par TuxGuitar source
    /// v6/v7 GPXFileSystem plus GPXInputStream (pick container, then score.gpif).
    /// @par Brief
    /// Detects header and delegates to ZIP or GP6 filesystem.
    /// @par Visibility
    /// public (library API).
    [[nodiscard]] ParseResult<std::vector<std::uint8_t>>
    extract_score_gpif(const std::vector<std::uint8_t> &file_data);

} // namespace libgp_parser
