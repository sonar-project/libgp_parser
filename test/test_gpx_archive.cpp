#include <libgp_parser/byte_string.hpp>
#include <libgp_parser/gpx_archive.hpp>

#include "test_helpers.hpp"

#include <catch2/catch_test_macros.hpp>
#include <miniz.h>

#include <cstring>
#include <span>
#include <vector>

using namespace libgp_parser;

namespace {

    constexpr int initial_zip_size = 1024;

    std::vector<std::uint8_t> make_minimal_gpx_zip() {
        const std::string score_xml = test::read_fixture("minimal_score.gpif");

        mz_zip_archive zip{};
        std::memset(&zip, 0, sizeof(zip));
        if (mz_zip_writer_init_heap(&zip, 0, initial_zip_size) == 0) {
            return {};
        }

        mz_zip_writer_add_mem(&zip, "Content/score.gpif", score_xml.data(), score_xml.size(),
                              MZ_DEFAULT_COMPRESSION);
        mz_zip_writer_add_mem(&zip, "VERSION", "7.0", 3, MZ_NO_COMPRESSION);

        void *heap = nullptr;
        size_t size = 0;
        if (mz_zip_writer_finalize_heap_archive(&zip, &heap, &size) == 0) {
            mz_zip_writer_end(&zip);
            return {};
        }
        mz_zip_writer_end(&zip);

        const std::span<const std::uint8_t> bytes{static_cast<const std::uint8_t *>(heap), size};
        std::vector<std::uint8_t> out(bytes.begin(), bytes.end());
        mz_free(heap);
        return out;
    }

} // namespace

TEST_CASE("GpxZipArchive extracts Content/score.gpif", "[gpx][zip]") {
    const std::vector<std::uint8_t> zip_bytes = make_minimal_gpx_zip();
    REQUIRE_FALSE(zip_bytes.empty());

    auto archive = GpxZipArchive::open(zip_bytes);
    REQUIRE(archive);
    REQUIRE(archive.value().contains(kGpxScorePathGp7));

    auto entry = archive.value().extract(kGpxScorePathGp7);
    REQUIRE(entry);
    const std::string xml = bytes_to_string(entry.value());
    const std::string_view xml_view{xml};
    REQUIRE(xml_view.contains("<Title>Test Song</Title>"));
}

TEST_CASE("GpxZipArchive extract_score_gpif", "[gpx][zip]") {
    const std::vector<std::uint8_t> zip_bytes = make_minimal_gpx_zip();
    auto archive = GpxZipArchive::open(zip_bytes);
    REQUIRE(archive);

    auto score = archive.value().extract_score_gpif();
    REQUIRE(score);
    REQUIRE(score.value().size() > 0);
}
