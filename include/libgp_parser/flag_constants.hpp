#pragma once

namespace FileFlag {
    // Often refers to triplet information or similar.
    constexpr int HAS_NUMBER_OF_TUPLETS{0x01};
    // Indicates whether the length of the bar is manually defined.
    constexpr int HAS_MEASURE_DURATION{0x02};
    // (Sometimes as HAS_BEGIN_REPEAT in early versions).
    // constexpr int HAS_BEGIN_REPEAT{0x03};
    constexpr int HAS_MARKER{0x08};
    // Indicates whether a key signature change is present.
    constexpr int HAS_TONALITY{0x10};
    // Indicates whether a double barline appears at the end of the measure.
    constexpr int HAS_DOUBLE_BARLINE{0x20};
    // Extended key signature information (often including accidentals).
    constexpr int HAS_KEY_SIGNATURE_EX{0x40};
} // namespace FileFlag