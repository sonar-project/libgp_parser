#pragma once

#include <cstdint>

namespace libgp_parser {

    /// Legacy tick count of a quarter note (TGDuration.QUARTER_TIME).
    inline constexpr long kQuarterTime = 960;

    /// Duration value constants (fractions of a whole), as in TGDuration.
    struct DurationValue {
        static constexpr int Whole = 1;
        static constexpr int Half = 2;
        static constexpr int Quarter = 4;
        static constexpr int Eighth = 8;
        static constexpr int Sixteenth = 16;
        static constexpr int ThirtySecond = 32;
        static constexpr int SixtyFourth = 64;
        static constexpr int Shortest = SixtyFourth;
    };

    /// MIDI-style velocities (TGVelocities).
    struct Velocity {
        static constexpr int Min = 15;
        static constexpr int Increment = 16;
        static constexpr int PianoPianissimo = Min;
        static constexpr int Pianissimo = Min + Increment;
        static constexpr int Piano = Min + (Increment * 2);
        static constexpr int MezzoPiano = Min + (Increment * 3);
        static constexpr int MezzoForte = Min + (Increment * 4);
        static constexpr int Forte = Min + (Increment * 5);
        static constexpr int Fortissimo = Min + (Increment * 6);
        static constexpr int ForteFortissimo = Min + (Increment * 7);
        static constexpr int Default = Forte;
    };

    /// Tuplet ratio (TGDivisionType).
    /// @par TuxGuitar source TGDivisionType (enters/times).
    struct DivisionType {
        int enters{1};
        int times{1};

        [[nodiscard]] long convert_time(const long time) const noexcept {
            return time * times / enters;
        }

        [[nodiscard]] bool operator==(const DivisionType &) const = default;
    };

    /// Note or rest duration (TGDuration).
    /// @par TuxGuitar source common/TuxGuitar-lib/.../TGDuration.java.
    struct Duration {
        int value{DurationValue::Quarter};
        bool dotted{false};
        bool double_dotted{false};
        DivisionType division{};

        /// Tick length using QUARTER_TIME, dots, and tuplet (getTime()).
        [[nodiscard]] long time() const noexcept {
            auto ticks = static_cast<long>(static_cast<float>(kQuarterTime) * (4.0F / value));
            if (dotted) {
                ticks += ticks / 2;
            } else if (double_dotted) {
                ticks += (ticks / 4) * 3;
            }
            return division.convert_time(ticks);
        }

        [[nodiscard]] bool operator==(const Duration &) const = default;
    };

    /// Time signature (TGTimeSignature).
    struct TimeSignature {
        int numerator{4};
        Duration denominator{};

        [[nodiscard]] bool operator==(const TimeSignature &) const = default;
    };

    /// Tempo in quarter-notes per minute (TGTempo, quarterValue).
    struct Tempo {
        int quarter_bpm{120};

        [[nodiscard]] bool operator==(const Tempo &) const = default;
    };

    /// Maps a tick length to a Duration, matching TGDuration.fromTime loosely.
    [[nodiscard]] inline Duration duration_from_time(const long ticks) {
        Duration found;
        for (int value = DurationValue::Whole; value <= DurationValue::Shortest; value *= 2) {
            found.value = value;
            if (ticks >= found.time()) {
                return found;
            }
        }
        found.value = DurationValue::Shortest;
        return found;
    }

} // namespace libgp_parser
