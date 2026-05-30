#pragma once

#include <cstdint>

namespace libgp_parser {

    /// RGB track color (three integer components as in GPX).
    /// @par TuxGuitar source GPXTrack.setColor / TGTrack.getColor.
    /// @par Brief Channel values 0–255 each.
    /// @par Visibility public (data type).
    struct RgbColor {
        std::uint8_t r{0};
        std::uint8_t g{0};
        std::uint8_t b{0};

        [[nodiscard]] bool operator==(const RgbColor &) const = default;
    };

    /// Default GM percussion MIDI channel (GPX channel 9).
    /// @par TuxGuitar source GPXDocument.DEFAULT_PERCUSSION_CHANNNEL.
    /// @par Brief Constant used by is_percussion().
    /// @par Visibility public.
    inline constexpr int kDefaultPercussionChannel = 9;

} // namespace libgp_parser
