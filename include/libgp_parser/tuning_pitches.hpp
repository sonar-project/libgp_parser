#pragma once

#include "libgp_parser/track.hpp"
#include <string>

namespace libgp_parser {

    /// Converts a MIDI note number to a name (e.g. 60 → "C4").
    /// @par TuxGuitar source
    /// No direct Java equivalent; display helper for GPXTrack/TGString in
    /// libgp_parser.
    /// @param note MIDI note number.
    /// @par Brief Display only, not part of the file format.
    /// @par Visibility public.
    std::string midi_note_to_name(int note);

    /// Fills tuning_notes and tuning_display after loading.
    /// @par TuxGuitar source
    /// Tuning from GPXDocumentReader.readTracks / GTP track; display strings are a
    /// C++ addition.
    /// @param track Track with tuning_pitches set.
    /// @par Brief Builds human-readable tuning strings.
    /// @par Visibility public.
    void finalize_track_tuning(libgp_parser::Track &track);

} // namespace libgp_parser
