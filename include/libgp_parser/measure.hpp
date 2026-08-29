#pragma once

#include "libgp_parser/beat.hpp"
#include "libgp_parser/duration.hpp"
#include "libgp_parser/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace libgp_parser {

    enum class TripletFeel : int {
        None = 1,
        Eighth = 2,
        Sixteenth = 3,
    };

    enum class Clef : int {
        Treble = 1,
        Bass = 2,
        Tenor = 3,
        Alto = 4,
    };

    /// Section marker (TGMarker).
    struct Marker {
        int measure{0};
        std::string title;
        RgbColor color{};

        [[nodiscard]] bool operator==(const Marker &) const = default;
    };

    /// Shared per-bar metadata (TGMeasureHeader).
    /// @par TuxGuitar source common/TuxGuitar-lib/.../TGMeasureHeader.java.
    struct MeasureHeader {
        int number{0};
        long start{kQuarterTime};
        TimeSignature time_signature{};
        Tempo tempo{};
        std::optional<Marker> marker;
        bool repeat_open{false};
        int repeat_alternative{0};
        int repeat_close{0};
        TripletFeel triplet_feel{TripletFeel::None};

        [[nodiscard]] bool has_marker() const noexcept { return marker.has_value(); }

        /// Length in ticks (numerator * denominator.time()).
        [[nodiscard]] long length() const noexcept {
            return static_cast<long>(time_signature.numerator) * time_signature.denominator.time();
        }

        [[nodiscard]] bool operator==(const MeasureHeader &) const = default;
    };

    /// One track bar (TGMeasure). header_index refers to Song::measure_headers.
    struct Measure {
        int header_index{0};
        Clef clef{Clef::Treble};
        int key_signature{0};
        std::vector<Beat> beats;

        [[nodiscard]] bool operator==(const Measure &) const = default;
    };

    [[nodiscard]] inline Beat *find_beat_at(Measure &measure, const long start) {
        return find_beat_at(measure.beats, start);
    }

    inline Beat &get_or_create_beat(Measure &measure, const long start) {
        return get_or_create_beat(measure.beats, start);
    }

} // namespace libgp_parser
