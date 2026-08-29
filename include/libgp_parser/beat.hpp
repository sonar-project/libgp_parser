#pragma once

#include "libgp_parser/duration.hpp"
#include "libgp_parser/effects.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace libgp_parser {

    inline constexpr int kMaxVoices = 2;

    enum class StrokeDirection : int {
        None = 0,
        Up = 1,
        Down = -1,
    };

    /// Brush stroke (TGStroke).
    struct Stroke {
        StrokeDirection direction{StrokeDirection::None};
        int value{0};

        [[nodiscard]] bool operator==(const Stroke &) const = default;
    };

    /// Pick stroke (TGPickStroke).
    struct PickStroke {
        StrokeDirection direction{StrokeDirection::None};

        [[nodiscard]] bool operator==(const PickStroke &) const = default;
    };

    /// Chord diagram (TGChord).
    struct Chord {
        std::string name;
        int first_fret{0};
        /// Per-string fret; -1 means unused.
        std::vector<int> frets;

        [[nodiscard]] int note_count() const noexcept {
            int count = 0;
            for (const int fret : frets) {
                if (fret >= 0) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] bool operator==(const Chord &) const = default;
    };

    /// One note (TGNote). value is fret number; string is 1-based.
    struct Note {
        int value{0};
        int velocity{Velocity::Default};
        int string{1};
        bool tied_note{false};
        NoteEffect effect{};

        [[nodiscard]] bool operator==(const Note &) const = default;
    };

    /// One voice on a beat (TGVoice).
    struct Voice {
        int index{0};
        Duration duration{};
        std::vector<Note> notes;
        bool empty{true};

        [[nodiscard]] bool is_rest() const noexcept { return notes.empty(); }

        void add_note(Note note) {
            notes.push_back(std::move(note));
            empty = false;
        }

        [[nodiscard]] bool operator==(const Voice &) const = default;
    };

    /// Beat at a tick start (TGBeat), with up to two voices.
    struct Beat {
        long start{kQuarterTime};
        std::array<Voice, kMaxVoices> voices{};
        std::optional<Chord> chord;
        std::string text;
        Stroke stroke{};
        PickStroke pick_stroke{};

        Beat() {
            voices[0].index = 0;
            voices[1].index = 1;
        }

        [[nodiscard]] Voice &voice(const int index) { return voices.at(static_cast<std::size_t>(index)); }
        [[nodiscard]] const Voice &voice(const int index) const {
            return voices.at(static_cast<std::size_t>(index));
        }

        [[nodiscard]] bool operator==(const Beat &) const = default;
    };

    [[nodiscard]] inline Beat *find_beat_at(std::vector<Beat> &beats, const long start) {
        for (Beat &beat : beats) {
            if (beat.start == start) {
                return &beat;
            }
        }
        return nullptr;
    }

    inline Beat &get_or_create_beat(std::vector<Beat> &beats, const long start) {
        if (Beat *existing = find_beat_at(beats, start)) {
            return *existing;
        }
        Beat beat;
        beat.start = start;
        beats.push_back(std::move(beat));
        return beats.back();
    }

} // namespace libgp_parser
