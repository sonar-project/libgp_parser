#include "libgp_parser/gpx_archive.hpp"
#include "miniz_common.h"

#include <miniz.h>

#include <cstring>
#include <utility>

namespace libgp_parser {
    namespace {

        ParseError zip_error(std::string_view context) {
            return {.code = ParseErrorCode::Zip, .message = std::string(context)};
        }

    } // namespace

    struct GpxZipArchive::ZipImpl {
        mz_zip_archive archive{};

        ZipImpl() { std::memset(&archive, 0, sizeof(archive)); }

        ~ZipImpl() { mz_zip_reader_end(&archive); }

        ZipImpl(const ZipImpl &) = delete;
        ZipImpl &operator=(const ZipImpl &) = delete;
        ZipImpl(ZipImpl &&) = default;
        ZipImpl &operator=(ZipImpl &&) = default;
    };

    GpxZipArchive::~GpxZipArchive() = default;

    GpxZipArchive::GpxZipArchive(GpxZipArchive &&other) noexcept = default;

    GpxZipArchive &GpxZipArchive::operator=(GpxZipArchive &&other) noexcept = default;

    GpxZipArchive::GpxZipArchive(std::unique_ptr<ZipImpl> zip) : zip_(std::move(zip)) {}

    ParseResult<GpxZipArchive> GpxZipArchive::open(const std::vector<std::uint8_t> &data) {
        if (data.empty()) {
            return ParseResult<GpxZipArchive>::failure(zip_error("ZIP data is empty"));
        }

        auto impl = std::make_unique<ZipImpl>();
        if (mz_zip_reader_init_mem(&impl->archive, data.data(), data.size(), 0) == MZ_FALSE) {
            return ParseResult<GpxZipArchive>::failure(zip_error("Failed to open ZIP archive"));
        }

        return ParseResult<GpxZipArchive>::success(GpxZipArchive(std::move(impl)));
    }

    bool GpxZipArchive::contains(const std::string_view path) const {
        if (!zip_) {
            return false;
        }
        return mz_zip_reader_locate_file(&zip_->archive, std::string(path).c_str(), nullptr, 0) >=
               0;
    }

    ParseResult<std::vector<std::uint8_t>>
    GpxZipArchive::extract(const std::string_view path) const {
        if (!zip_) {
            return ParseResult<std::vector<std::uint8_t>>::failure(
                zip_error("ZIP archive is not open"));
        }

        const int index =
            mz_zip_reader_locate_file(&zip_->archive, std::string(path).c_str(), nullptr, 0);
        if (index < 0) {
            return ParseResult<std::vector<std::uint8_t>>::failure(
                {.code = ParseErrorCode::NotFound,
                 .message = std::string("ZIP entry not found: ") + std::string(path)});
        }

        const mz_zip_archive_file_stat stat = [&] {
            mz_zip_archive_file_stat zipstat{};
            mz_zip_reader_file_stat(&zip_->archive, static_cast<mz_uint>(index), &zipstat);
            return zipstat;
        }();

        std::vector<std::uint8_t> buffer(static_cast<std::size_t>(stat.m_uncomp_size));
        if (mz_zip_reader_extract_to_mem(&zip_->archive, static_cast<mz_uint>(index), buffer.data(),
                                         buffer.size(), 0) == MZ_FALSE) {
            return ParseResult<std::vector<std::uint8_t>>::failure(
                zip_error("Failed to decompress ZIP entry"));
        }

        return ParseResult<std::vector<std::uint8_t>>::success(std::move(buffer));
    }

    ParseResult<std::vector<std::uint8_t>> GpxZipArchive::extract_score_gpif() const {
        if (contains(kGpxScorePathGp7)) {
            return extract(kGpxScorePathGp7);
        }
        if (contains(kGpxScorePathGp6)) {
            return extract(kGpxScorePathGp6);
        }
        return ParseResult<std::vector<std::uint8_t>>::failure(
            {.code = ParseErrorCode::NotFound, .message = "score.gpif not found in archive"});
    }

} // namespace libgp_parser
