#pragma once

#include <cstdint>
#include <cstring>
#include <span>

namespace libgp_parser {

    /// BCFS magic for GP6 containers (see GPXFileSystem.HEADER_BCFS).
    inline constexpr std::uint32_t kGpxHeaderBcfs = 1397113666U; // "BCFS"
    /// BCFZ magic for compressed GP6 containers (HEADER_BCFZ).
    inline constexpr std::uint32_t kGpxHeaderBcfz = 1514554178U; // "BCFZ"

    inline constexpr std::uint32_t kZipHeader =
        0x04034B50U; // "PK\003\004" ZIP local file header signature.

    /// Returns true for a BCFS or BCFZ header.
    /// @par TuxGuitar source GPXFileSystem.isSupportedHeader().
    /// @par Brief true for GP6 .gpx containers.
    /// @par Visibility public.
    [[nodiscard]] inline bool is_gpx_gp6_header(std::uint32_t header) noexcept {
        return header == kGpxHeaderBcfs || header == kGpxHeaderBcfz;
    }

    /// Returns true for ZIP magic "PK" (GP7 .gp).
    /// @par TuxGuitar source GP7 uses ZIP instead of BCFS.
    /// @par Brief ZIP container detection.
    /// @par Visibility public.
    [[nodiscard]] inline bool is_zip_header(std::span<const std::uint8_t> data) noexcept {
        if (data.size() < 4) {
            return false;
        }

        std::uint32_t header{0};
        std::memcpy(&header, data.data(), sizeof(header));

        return header == kZipHeader;
    }

    /// Reads a 32-bit little-endian value from a byte buffer.
    /// @par TuxGuitar source GPXFileSystem.getInteger(bytes, offset).
    /// @par Brief Helper for reading headers and fields.
    /// @par Visibility public.
    [[nodiscard]] std::uint32_t read_le_u32(std::span<const std::uint8_t> data,
                                            std::size_t offset = 0);

} // namespace libgp_parser
