#include "libgp_parser/tempo.hpp"

namespace libgp_parser {

int apply_gpx_tempo_modifier(const int bpm, const int note_value) noexcept {
    switch (note_value) {
    case 1:
        return bpm / 2;
    case 3:
        return bpm + (bpm / 2);
    case 4:
        return bpm * 2;
    case 5:
        return bpm + (bpm * 2);
    default:
        return bpm;
    }
}

} // namespace libgp_parser
