#include <libgp_parser/load_song.hpp>

#include "test_gpx_zip_fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace libgp_parser;

TEST_CASE("load_song reads metadata from .gpx zip", "[load]") {
    const std::filesystem::path path = test::write_temp_gpx_fixture();

    auto result = load_song(path);
    REQUIRE(result);
    REQUIRE(result.value().name() == "Test Song");
    REQUIRE(result.value().artist() == "Test Artist");
    REQUIRE(result.value().tempo_bpm == 120);
    REQUIRE(result.value().track_count() == 0);

    std::error_code error_code;
    std::filesystem::remove(path, error_code);
}

TEST_CASE("load_song fails on missing file", "[load]") {
    auto result = load_song("nonexistent_file.gpx");
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == ParseErrorCode::Io);
}
