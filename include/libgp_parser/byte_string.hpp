#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace libgp_parser {

    /// Copies byte data into a text string (no pointer casts).
    [[nodiscard]] inline std::string bytes_to_string(std::span<const std::uint8_t> bytes) {
        return {bytes.begin(), bytes.end()};
    }

    [[nodiscard]] inline std::string bytes_to_string(const std::vector<std::uint8_t> &bytes) {
        return bytes_to_string(std::span{bytes});
    }

} // namespace libgp_parser
