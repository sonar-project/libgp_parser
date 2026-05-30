#pragma once

#include "libgp_parser/error.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace libgp_parser {

    /// GP6 container (.gpx) with BCFS or BCFZ header.
    /// @par TuxGuitar source
    /// common/TuxGuitar-gpx/.../v6/GPXFileSystem.java (load, getFileContents).
    /// @par Brief
    /// Reads the embedded filesystem and returns score.gpif.
    /// @par Visibility
    /// public (class).
    class GpxGp6FileSystem {
      public:
        /// Parses BCFS/BCFZ data from a complete .gpx file.
        /// @par TuxGuitar source GPXFileSystem.load(header, buffer).
        /// @par Brief Builds the internal file list.
        /// @par Visibility public.
        [[nodiscard]] static ParseResult<GpxGp6FileSystem> load(std::vector<std::uint8_t> data);

        /// Returns true if a file exists in the GP6 container.
        /// @par TuxGuitar source getFileNames / getFileContents.
        /// @par Brief true when the name is found.
        /// @par Visibility public.
        [[nodiscard]] bool contains(std::string_view file_name) const;

        /// Returns the contents of a file in the container.
        /// @par TuxGuitar source getFileContents(fileName).
        /// @par Brief Raw bytes of the embedded file.
        /// @par Visibility public.
        [[nodiscard]] ParseResult<std::vector<std::uint8_t>>
        extract(std::string_view file_name) const;

        /// Extracts score.gpif from the GP6 filesystem.
        /// @par TuxGuitar source getFileContentsAsStream("score.gpif").
        /// @par Brief Default GP6 score path.
        /// @par Visibility public.
        [[nodiscard]] ParseResult<std::vector<std::uint8_t>> extract_score_gpif() const;

      private:
        explicit GpxGp6FileSystem(
            std::vector<std::pair<std::string, std::vector<std::uint8_t>>> files);

        std::vector<std::pair<std::string, std::vector<std::uint8_t>>> files_;
    };

} // namespace libgp_parser
