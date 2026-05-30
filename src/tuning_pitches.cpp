#include "libgp_parser/track.hpp"

#include <array>
#include <string>
#include <string_view>

namespace libgp_parser {
    constexpr int kOktaveSize = 12;

    std::string midi_note_to_name(int note) {
        static constexpr std::array<std::string_view, 12> kNoteNames = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        if (note < 0) {
            return "?";
        }

        const auto index = static_cast<std::size_t>(note % static_cast<int>(kOktaveSize));
        std::string name{kNoteNames.at(index)};
        int octave = (note / kOktaveSize) - 1;
        return name + std::to_string(octave);
    }

    // Call after a track has finished loading to fill display tuning fields.
    void finalize_track_tuning(libgp_parser::Track &track) {
        std::string display_str;

        for (int pitch : track.tuning_pitches) {
            // Skip drums / invalid values (MIDI 0)
            if (pitch <= 0) {
                continue;
            }

            std::string note_name = midi_note_to_name(pitch);
            track.tuning_notes.push_back(note_name);

            // Note name only, no octave, for compact display (e.g. "D" not "D2")
            // Strip trailing digits from names like "C#".
            std::string::size_type pos = note_name.find_first_of("0123456789");
            std::string pure_note = note_name.substr(0, pos);

            if (!display_str.empty()) {
                display_str += " ";
            }
            display_str += pure_note;
        }

        track.tuning_display = display_str;
    }
} // namespace libgp_parser