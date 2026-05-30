#pragma once

namespace libgp_parser {

    /// Converts GPX tempo using the note-value factor to BPM.
    /// @par TuxGuitar source
    /// common/TuxGuitar-gpx/.../GPXDocumentParser.java, parseMasterBars() (Tempo
    /// Value[1]).
    /// @param bpm Base BPM from Value[0].
    /// @param note_value Value[1] (4 = quarter note → double).
    /// @par Brief
    /// Same switch logic as in Java.
    /// @par Visibility
    /// public (library API).
    [[nodiscard]] int apply_gpx_tempo_modifier(int bpm, int note_value) noexcept;

} // namespace libgp_parser
