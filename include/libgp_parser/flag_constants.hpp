#pragma once

namespace FileFlag {
    /// Time-signature numerator present (GP measure header bit 0x01).
    constexpr int HAS_TIME_NUMERATOR{0x01};
    /// Time-signature denominator present (0x02).
    constexpr int HAS_TIME_DENOMINATOR{0x02};
    /// Repeat-open barline (0x04).
    constexpr int HAS_REPEAT_OPEN{0x04};
    /// Repeat-close count present (0x08).
    constexpr int HAS_REPEAT_CLOSE{0x08};
    /// Alternate ending present (0x10).
    constexpr int HAS_REPEAT_ALTERNATIVE{0x10};
    /// Marker present (0x20).
    constexpr int HAS_MARKER{0x20};
    /// Key signature present (0x40).
    constexpr int HAS_KEY_SIGNATURE{0x40};
} // namespace FileFlag
