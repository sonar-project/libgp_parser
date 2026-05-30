#include <libgp_parser/binary_reader.hpp>
#include <libgp_parser/gtp_reader.hpp>
#include <libgp_parser/load_song.hpp>

#include "test_helpers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>

using namespace libgp_parser;

namespace {

    std::vector<std::uint8_t> read_file(const std::filesystem::path &path) {
        std::ifstream inputStream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(inputStream), std::istreambuf_iterator<char>()};
    }

    std::vector<std::uint8_t> load_test_file(const std::string &filename) {
        std::string content = test::read_fixture(filename);
        return {content.begin(), content.end()};
    }

} // namespace

void verify_song_data_GP3(const Song &song) {
    REQUIRE(song.name() == "Example File GP3");
    REQUIRE(song.artist() == "SonarPractice");
    REQUIRE(song.tempo_bpm == 120);
    REQUIRE(song.track_count() > 0);
}

void verify_song_data_GP4(const Song &song) {
    REQUIRE(song.name() == "Example File GP4");
    REQUIRE(song.artist() == "SonarPractice");
    REQUIRE(song.tempo_bpm == 120);
    REQUIRE(song.track_count() > 0);
}

void verify_song_data_GP5(const Song &song) {
    REQUIRE(song.name() == "Example File GP5");
    REQUIRE(song.artist() == "SonarPractice");
    REQUIRE(song.tempo_bpm == 120);
    REQUIRE(song.track_count() > 0);
}

void verify_song_data_GPX(const Song &song) {
    REQUIRE(song.name() == "Example File 1");
    REQUIRE(song.artist() == "SonarPractice");
    REQUIRE(song.tempo_bpm == 120);
    REQUIRE(song.track_count() > 0);
}

// Load file (CMake points tests at tests/data/)
TEST_CASE("Integration Test: Real GP3 file parsing", "[gtp][integration]") {
    const auto file_data = load_test_file("testfile.gp3");
    REQUIRE_FALSE(file_data.empty());
    REQUIRE(is_gtp_file(file_data));

    auto result = load_gtp_song(file_data);
    REQUIRE(result);
    verify_song_data_GP3(result.value());
}

TEST_CASE("Integration Test: Real GP4 file parsing", "[gtp][integration]") {
    const auto file_data = load_test_file("testfile.gp4");
    REQUIRE_FALSE(file_data.empty());
    REQUIRE(is_gtp_file(file_data));

    auto result = load_gtp_song(file_data);
    INFO(result.error().message);
    REQUIRE(result);

    const auto &song = result.value();
    verify_song_data_GP4(song);
}

TEST_CASE("Integration Test: Real GP5 file parsing", "[gtp][integration]") {
    const auto file_data = load_test_file("testfile.gp5");

    REQUIRE_FALSE(file_data.empty());
    REQUIRE(is_gtp_file(file_data));

    auto result = load_gtp_song(file_data);

    INFO(result.error().message);
    REQUIRE(result);

    const auto &song = result.value();
    verify_song_data_GP5(song);
}

TEST_CASE("Integration Test: Real GPX file parsing", "[gpx][integration]") {
    const std::filesystem::path path = test::test_data_dir() / "testfile.gpx";
    if (!std::filesystem::exists(path)) {
        SKIP("testfile.gpx not found in test data directory");
    }

    auto result = load_song(path);
    INFO(result.error().message);
    REQUIRE(result);

    const Song &song = result.value();
    verify_song_data_GPX(song);
}