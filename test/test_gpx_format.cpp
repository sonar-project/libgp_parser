#include <libgp_parser/bit_constants.hpp>
#include <libgp_parser/gpx_format.hpp>
#include <libgp_parser/gtp_reader.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

#include <span>

using namespace libgp_parser;

TEST_CASE("Format header detection", "[format]") {
    REQUIRE(is_gpx_gp6_header(kGpxHeaderBcfs));
    REQUIRE(is_gpx_gp6_header(kGpxHeaderBcfz));
    REQUIRE_FALSE(is_gpx_gp6_header(0x504B0304U)); // ZIP magic

    const std::array<std::uint8_t, 4> zip_magic{0x50, 0x4B, 0x03, 0x04};
    REQUIRE(is_zip_header(zip_magic));

    std::vector<std::uint8_t> gtp(Constants::ShiftByte30 + 1);
    gtp[0] = Constants::ShiftByte30;

    constexpr auto ver = std::to_array("FICHIER GUITAR PRO v5.00");

    std::span<std::uint8_t> gtp_span(gtp);
    auto target = gtp_span.subspan(1, sizeof(ver) - 1);

    std::copy_n(ver.begin(), ver.size() - 1, target.begin());

    REQUIRE(is_gtp_file(gtp));
}
