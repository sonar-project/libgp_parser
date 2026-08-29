#pragma once

#include "libgp_parser/duration.hpp"

#include <optional>
#include <vector>

namespace libgp_parser {

    /// One point on a bend or tremolo-bar curve.
    struct EffectPoint {
        int position{0};
        int value{0};

        [[nodiscard]] bool operator==(const EffectPoint &) const = default;
    };

    /// Pitch bend (TGEffectBend).
    struct EffectBend {
        static constexpr int SemitoneLength = 1;
        static constexpr int MaxPositionLength = 12;

        std::vector<EffectPoint> points;

        [[nodiscard]] bool empty() const noexcept { return points.empty(); }
        [[nodiscard]] bool operator==(const EffectBend &) const = default;
    };

    /// Whammy / tremolo bar (TGEffectTremoloBar).
    struct EffectTremoloBar {
        static constexpr int MaxPositionLength = 12;
        static constexpr int MaxValueLength = 12;

        std::vector<EffectPoint> points;

        [[nodiscard]] bool empty() const noexcept { return points.empty(); }
        [[nodiscard]] bool operator==(const EffectTremoloBar &) const = default;
    };

    /// Harmonic type (TGEffectHarmonic).
    enum class HarmonicType : int {
        None = 0,
        Natural = 1,
        Artificial = 2,
        Tapped = 3,
        Pinch = 4,
        Semi = 5,
    };

    /// Natural harmonic fret/offset table (TGEffectHarmonic.NATURAL_FREQUENCIES).
    inline constexpr int kNaturalFrequencies[][2] = {
        {12, 12}, {9, 28}, {5, 24}, {7, 19}, {4, 28}, {3, 31},
    };

    /// Harmonic (TGEffectHarmonic).
    struct EffectHarmonic {
        HarmonicType type{HarmonicType::None};
        int data{0};

        [[nodiscard]] bool operator==(const EffectHarmonic &) const = default;
    };

    /// Grace-note transition (TGEffectGrace).
    enum class GraceTransition : int {
        None = 0,
        Slide = 1,
        Bend = 2,
        Hammer = 3,
    };

    /// Grace duration encoding used by GP (TGEffectGrace.DURATION_*).
    struct GraceDuration {
        static constexpr int SixtyFourth = 1;
        static constexpr int ThirtySecond = 2;
        static constexpr int Sixteenth = 3;
    };

    /// Grace note (TGEffectGrace).
    struct EffectGrace {
        int fret{0};
        int duration{GraceDuration::SixtyFourth};
        int dynamic{Velocity::Default};
        GraceTransition transition{GraceTransition::None};
        bool on_beat{false};
        bool dead{false};

        [[nodiscard]] bool operator==(const EffectGrace &) const = default;
    };

    /// Trill (TGEffectTrill).
    struct EffectTrill {
        int fret{0};
        Duration duration{};

        [[nodiscard]] bool operator==(const EffectTrill &) const = default;
    };

    /// Tremolo picking (TGEffectTremoloPicking).
    struct EffectTremoloPicking {
        Duration duration{};

        [[nodiscard]] bool operator==(const EffectTremoloPicking &) const = default;
    };

    /// Combined note effects (TGNoteEffect).
    /// @par TuxGuitar source common/TuxGuitar-lib/.../TGNoteEffect.java.
    struct NoteEffect {
        std::optional<EffectBend> bend;
        std::optional<EffectTremoloBar> tremolo_bar;
        std::optional<EffectHarmonic> harmonic;
        std::optional<EffectGrace> grace;
        std::optional<EffectTrill> trill;
        std::optional<EffectTremoloPicking> tremolo_picking;

        bool vibrato{false};
        bool dead_note{false};
        bool slide{false};
        bool hammer{false};
        bool ghost_note{false};
        bool accentuated_note{false};
        bool heavy_accentuated_note{false};
        bool palm_mute{false};
        bool staccato{false};
        bool tapping{false};
        bool slapping{false};
        bool popping{false};
        bool fade_in{false};
        bool let_ring{false};

        [[nodiscard]] bool has_bend() const noexcept { return bend.has_value() && !bend->empty(); }
        [[nodiscard]] bool has_any() const noexcept {
            return has_bend() || tremolo_bar.has_value() || harmonic.has_value() ||
                   grace.has_value() || trill.has_value() || tremolo_picking.has_value() ||
                   vibrato || dead_note || slide || hammer || ghost_note || accentuated_note ||
                   heavy_accentuated_note || palm_mute || let_ring || staccato || tapping ||
                   slapping || popping || fade_in;
        }

        [[nodiscard]] bool operator==(const NoteEffect &) const = default;
    };

} // namespace libgp_parser
