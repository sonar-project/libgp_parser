#pragma once

#include "libgp_parser/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace libgp_parser {

    /// One track (instrument) in a score.
    /// @par TuxGuitar source
    /// common/TuxGuitar-gpx/.../score/GPXTrack.java; converted to TGTrack in
    /// GPXDocumentParser.parseTracks() or GTP track reading in GTPInputStream.
    /// @par Brief
    /// Name, tuning (MIDI), GM channels, capo, and color.
    /// @par Visibility
    /// public (data type).
    struct Track {
        int id{0};
        /// 1-based index within the song (set when building the final Song model).
        int number{0};

        std::string name;

        /// MIDI note numbers per string; empty for percussion or not-yet-parsed
        /// tracks.
        std::vector<int> tuning_pitches;

        std::vector<std::string> tuning_notes; // e.g. {"D2", "G2", "C3", ...}
        std::string tuning_display;            // e.g. "D G C F A D"

        std::optional<RgbColor> color;

        int capo{0};

        int gm_program{0};
        int gm_channel1{0};
        int gm_channel2{0};

        /// Assigned mixer channel after deduplication (TG channel id); -1 if unknown.
        int channel_id{-1};

        /// Returns true if this track is percussion (GM channel 9).
        /// @par TuxGuitar source GPXDocumentParser.parseTracks(), percussion branch.
        /// @par Brief Same rule as DEFAULT_PERCUSSION_CHANNEL.
        /// @par Visibility public.
        [[nodiscard]] bool is_percussion() const noexcept {
            return gm_channel1 == kDefaultPercussionChannel;
        }

        [[nodiscard]] bool operator==(const Track &) const = default;
    };

} // namespace libgp_parser
