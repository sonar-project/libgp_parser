#include "libgp_parser/tempo.hpp"

namespace libgp_parser {
    int apply_gpx_tempo_modifier(const TempoParams &params) noexcept {
        switch (params.note_value) {
        case NoteType::Half:
            return params.bpm / 2;
        case NoteType::DottedHalf:
            return params.bpm + (params.bpm / 2);
        case NoteType::Double:
            return params.bpm * 2;
        case NoteType::Triple:
            return params.bpm + (params.bpm * 2);
        default:
            return params.bpm;
        }
    }

} // namespace libgp_parser