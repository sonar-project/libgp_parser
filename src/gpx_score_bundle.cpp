#include "libgp_parser/gpx_score_bundle.hpp"

#include "libgp_parser/gpx_archive.hpp"
#include "libgp_parser/gpx_format.hpp"
#include "libgp_parser/gpx_gp6_filesystem.hpp"

namespace libgp_parser {

    ParseResult<std::vector<std::uint8_t>>
    extract_score_gpif(const std::vector<std::uint8_t> &file_data) {
        if (file_data.size() < 4) {
            return ParseResult<std::vector<std::uint8_t>>::failure(
                {.code = ParseErrorCode::Unsupported, .message = "GPX file too short"});
        }

        const std::uint32_t header = read_le_u32(file_data);

        if (is_gpx_gp6_header(header)) {
            auto payloadFs = GpxGp6FileSystem::load(file_data);
            if (!payloadFs) {
                return ParseResult<std::vector<std::uint8_t>>::failure(payloadFs.error());
            }
            return payloadFs.value().extract_score_gpif();
        }

        if (is_zip_header(file_data)) {
            auto archive = GpxZipArchive::open(file_data);
            if (!archive) {
                return ParseResult<std::vector<std::uint8_t>>::failure(archive.error());
            }
            return archive.value().extract_score_gpif();
        }

        return ParseResult<std::vector<std::uint8_t>>::failure(
            {.code = ParseErrorCode::Unsupported,
             .message = "Unknown GPX container (expected ZIP, BCFS, or BCFZ)"});
    }

} // namespace libgp_parser
