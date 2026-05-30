#pragma once

#include "libgp_parser/error.hpp"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace libgp_parser {

    /// Score path inside a GP7 ZIP (see GPXFileSystem.RESOURCE_SCORE).
    inline constexpr std::string_view kGpxScorePathGp7 = "Content/score.gpif";
    /// Score path in GP6 ZIP-like containers.
    inline constexpr std::string_view kGpxScorePathGp6 = "score.gpif";

    /// Read-only ZIP wrapper for .gp / GP7 archives (miniz).
    /// @par TuxGuitar source
    /// common/TuxGuitar-gpx/.../v7/GPXFileSystem.java (ZIP,
    /// getFileContentsAsStream).
    /// @par Brief
    /// Opens ZIP data and extracts score.gpif.
    /// @par Visibility
    /// public (class).
    class GpxZipArchive {
      public:
        GpxZipArchive() = default;
        ~GpxZipArchive();

        GpxZipArchive(const GpxZipArchive &) = delete;
        GpxZipArchive &operator=(const GpxZipArchive &) = delete;
        GpxZipArchive(GpxZipArchive &&other) noexcept;
        GpxZipArchive &operator=(GpxZipArchive &&other) noexcept;

        /// Opens a ZIP from in-memory bytes.
        /// @par TuxGuitar source GPXFileSystem.load(InputStream).
        /// @par Brief Loads the full archive buffer.
        /// @par Visibility public.
        [[nodiscard]] static ParseResult<GpxZipArchive> open(const std::vector<std::uint8_t> &data);

        /// Returns true if a path exists in the archive.
        /// @par TuxGuitar source ZIP entry lookup in GPXFileSystem.
        /// @par Brief true when the file is present.
        /// @par Visibility public.
        [[nodiscard]] bool contains(std::string_view path) const;

        /// Extracts one entry by exact path.
        /// @par TuxGuitar source getFileContentsAsStream(resource).
        /// @par Brief Raw bytes of a ZIP entry.
        /// @par Visibility public.
        [[nodiscard]] ParseResult<std::vector<std::uint8_t>> extract(std::string_view path) const;

        /// Extracts score.gpif (GP7 or GP6 path).
        /// @par TuxGuitar source RESOURCE_SCORE or score.gpif.
        /// @par Brief First matching score path found.
        /// @par Visibility public.
        [[nodiscard]] ParseResult<std::vector<std::uint8_t>> extract_score_gpif() const;

      private:
        struct ZipImpl;
        explicit GpxZipArchive(std::unique_ptr<ZipImpl> zip);

        std::unique_ptr<ZipImpl> zip_;
    };

} // namespace libgp_parser
