#pragma once

#include <string>

namespace libgp_parser {

    /// Track lyrics (TGLyric). from is 1-based measure number.
    /// @par TuxGuitar source common/TuxGuitar-lib/.../TGLyric.java.
    struct Lyric {
        int from{1};
        std::string text;

        [[nodiscard]] bool empty() const noexcept { return text.empty(); }
        [[nodiscard]] bool operator==(const Lyric &) const = default;
    };

} // namespace libgp_parser
