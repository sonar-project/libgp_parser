#pragma once

#include "libgp_parser/duration.hpp"
#include "libgp_parser/song.hpp"

#include <vector>

namespace libgp_parser {

    /// MIDI note number for a tablature note (MidiSequenceParser.addNotes).
    /// @par TuxGuitar source
    /// common/TuxGuitar-lib/.../player/base/MidiSequenceParser.java, addNotes()
    /// (`transpose` is 0 here): capo + fret + string pitch.
    /// Percussion uses the same formula (no extra conversion).
    /// @par Brief Capo + fret + open-string MIDI pitch.
    /// @par Visibility public.
    [[nodiscard]] inline int midi_pitch(const Track &track, const Note &note) {
        return track.capo + note.value + track.string_pitch(note.string);
    }

    /// Converts legacy ticks to milliseconds at a quarter-note BPM.
    /// @par TuxGuitar source TGTempo.getTicksInMillis(ticks).
    /// @par Brief `60000 * ticks / QUARTER_TIME / bpm` (integer division).
    /// @par Visibility public.
    [[nodiscard]] inline long ticks_to_ms(const long ticks, const int quarter_bpm) {
        if (quarter_bpm <= 0) {
            return 0;
        }
        return 60L * 1000L * ticks / kQuarterTime / quarter_bpm;
    }

    /// One measure in playback order after expanding repeats.
    /// @par TuxGuitar source MidiSequenceHelper.MidiMeasureHelper (index + move).
    /// @par Brief Zero-based header index, repeat pass, and tick shift.
    /// @par Visibility public.
    struct PlaybackMeasure {
        int header_index{0};
        int pass{0};
        long repeat_move{0};

        [[nodiscard]] bool operator==(const PlaybackMeasure &) const = default;
    };

    /// Expands measure headers into playback order (open/close/alternate endings).
    /// @par TuxGuitar source
    /// common/TuxGuitar-lib/.../player/base/MidiRepeatController.java used by
    /// MidiSequenceParser.parse().
    /// @param s_header 1-based first measure in a practice loop, or -1 for the whole song.
    /// @param e_header 1-based last measure in a practice loop, or -1 for the whole song.
    /// @par Brief Linearised `(header_index, pass)` list for loops and full playback.
    /// @par Visibility public.
    [[nodiscard]] std::vector<PlaybackMeasure> expand_repeats(const Song &song, int s_header = -1,
                                                              int e_header = -1);

} // namespace libgp_parser
