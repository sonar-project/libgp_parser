#include "libgp_parser/gpx_score_bundle.hpp"

#include "libgp_parser/gpx_archive.hpp"
#include "libgp_parser/gpx_edit_lock.hpp"
#include "libgp_parser/gpx_format.hpp"
#include "libgp_parser/gpx_gp6_filesystem.hpp"

#include <string>

namespace libgp_parser {
    namespace {

        ParseResult<std::vector<std::uint8_t>>
        maybe_unlock_gpif(const GpxZipArchive &archive, std::vector<std::uint8_t> gpif) {
            if (looks_like_gpif_xml(gpif)) {
                return ParseResult<std::vector<std::uint8_t>>::success(std::move(gpif));
            }

            if (archive.contains(kGpxEditLockedPath)) {
                auto lock = archive.extract(kGpxEditLockedPath);
                if (!lock) {
                    return ParseResult<std::vector<std::uint8_t>>::failure(lock.error());
                }
                auto decrypted = decrypt_edit_locked_gpif(gpif, lock.value());
                if (!decrypted) {
                    return decrypted;
                }
                if (!looks_like_gpif_xml(decrypted.value())) {
                    return ParseResult<std::vector<std::uint8_t>>::failure(
                        {.code = ParseErrorCode::Unsupported,
                         .message =
                             "Decrypted edit-locked score.gpif is not XML"});
                }
                return decrypted;
            }

            return ParseResult<std::vector<std::uint8_t>>::failure(
                {.code = ParseErrorCode::Unsupported,
                 .message = "score.gpif is not XML; file may be locked or corrupt"});
        }

        [[nodiscard]] bool is_supported_gp_zip_version(std::string_view text) noexcept {
            // TuxGuitar's reader accepts any ZIP with score.gpif; the detector only
            // lists "7.0". GP8 still writes VERSION "7.0" or "8.0".
            return text.starts_with("7.") || text.starts_with("8.");
        }

    } // namespace

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
            auto gpif = payloadFs.value().extract_score_gpif();
            if (!gpif) {
                return gpif;
            }
            if (!looks_like_gpif_xml(gpif.value())) {
                return ParseResult<std::vector<std::uint8_t>>::failure(
                    {.code = ParseErrorCode::Unsupported,
                     .message = "score.gpif is not XML; file may be locked or corrupt"});
            }
            return gpif;
        }

        if (is_zip_header(file_data)) {
            auto archive = GpxZipArchive::open(file_data);
            if (!archive) {
                return ParseResult<std::vector<std::uint8_t>>::failure(archive.error());
            }
            if (archive.value().contains("VERSION")) {
                auto version = archive.value().extract("VERSION");
                if (version) {
                    std::string text(version.value().begin(), version.value().end());
                    // Trim trailing CR/LF/NUL that some writers append.
                    while (!text.empty() &&
                           (text.back() == '\n' || text.back() == '\r' || text.back() == '\0')) {
                        text.pop_back();
                    }
                    if (text.size() >= 3) {
                        text.resize(3);
                    }
                    if (!is_supported_gp_zip_version(text)) {
                        return ParseResult<std::vector<std::uint8_t>>::failure(
                            {.code = ParseErrorCode::Unsupported,
                             .message = "Unsupported GP ZIP version: " + text});
                    }
                }
            }
            auto gpif = archive.value().extract_score_gpif();
            if (!gpif) {
                return gpif;
            }
            return maybe_unlock_gpif(archive.value(), std::move(gpif.value()));
        }

        return ParseResult<std::vector<std::uint8_t>>::failure(
            {.code = ParseErrorCode::Unsupported,
             .message = "Unknown GPX container (expected ZIP, BCFS, or BCFZ)"});
    }

} // namespace libgp_parser
