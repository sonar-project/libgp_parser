#pragma once

#include "libgp_parser/error.hpp"

#include <string_view>

namespace libgp_parser {

    /// Reads initial tempo (BPM) from Tempo automations in MasterTrack.
    /// @par TuxGuitar source
    /// GPXDocumentReader.readAutomations() and GPXDocument.getAutomation("Tempo",
    /// 0); tempo conversion as in GPXDocumentParser.parseMasterBars().
    /// @par Brief
    /// Finds the matching Tempo automation for bar 0.
    /// @par Visibility
    /// public (library API).
    [[nodiscard]] ParseResult<int> parse_gpx_initial_tempo(std::string_view xml);

} // namespace libgp_parser
