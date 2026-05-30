#include "libgp_parser/gpx_format.hpp"
#include "libgp_parser/bit_constants.hpp"

namespace libgp_parser {

    std::uint32_t read_le_u32(const std::span<const std::uint8_t> data, const std::size_t offset) {
        if (data.size() < offset + 4) {
            return 0;
        }
        return static_cast<std::uint32_t>(data[offset]) |
               (static_cast<std::uint32_t>(data[offset + 1]) << Constants::ShiftByte8) |
               (static_cast<std::uint32_t>(data[offset + 2]) << Constants::ShiftByte16) |
               (static_cast<std::uint32_t>(data[offset + 3]) << Constants::ShiftByte24);
    }

} // namespace libgp_parser
