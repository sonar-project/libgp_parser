#include <libgp_parser/binary_reader.hpp>
#include <libgp_parser/gtp_reader.hpp>
#include <libgp_parser/load_song.hpp>

#include "test_helpers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>
#include <vector>

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

const Note &first_note(const Song &song) {
    REQUIRE_FALSE(song.tracks.empty());
    REQUIRE_FALSE(song.tracks.front().measures.empty());
    REQUIRE_FALSE(song.tracks.front().measures.front().beats.empty());
    const Beat &beat = song.tracks.front().measures.front().beats.front();
    REQUIRE_FALSE(beat.voice(0).notes.empty());
    return beat.voice(0).notes.front();
}

void verify_example_track(const Song &song, const int expected_fret,
                          const std::vector<int> &expected_tuning) {
    REQUIRE(song.tempo_bpm == 120);
    REQUIRE(song.measure_count() == 1);
    REQUIRE(song.track_count() == 1);
    REQUIRE(song.channels.size() == 1);
    REQUIRE(song.measure_headers.front().time_signature.numerator == 4);
    REQUIRE(song.measure_headers.front().time_signature.denominator.value == DurationValue::Quarter);
    REQUIRE_FALSE(song.measure_headers.front().repeat_open);
    REQUIRE(song.tracks.front().name == "Overdriven Guitar");
    REQUIRE(song.tracks.front().gm_program == 29);
    REQUIRE(song.tracks.front().channel_id == 1);
    REQUIRE(song.tracks.front().tuning_pitches == expected_tuning);
    REQUIRE(song.tracks.front().measures.size() == 1);
    const Note &note = first_note(song);
    REQUIRE(note.string == 6);
    REQUIRE(note.value == expected_fret);
    REQUIRE(note.velocity == Velocity::MezzoForte);
}

void verify_song_data_GP3(const Song &song) {
    REQUIRE(song.name() == "Example File GP3");
    REQUIRE(song.artist() == "SonarPractice");
    verify_example_track(song, 1, {64, 59, 55, 50, 45, 40});
}

void verify_song_data_GP4(const Song &song) {
    REQUIRE(song.name() == "Example File GP4");
    REQUIRE(song.artist() == "SonarPractice");
    verify_example_track(song, 1, {64, 59, 55, 50, 45, 40});
}

void verify_song_data_GP5(const Song &song) {
    REQUIRE(song.name() == "Example File GP5");
    REQUIRE(song.artist() == "SonarPractice");
    verify_example_track(song, 3, {62, 57, 53, 48, 43, 38});
}

void verify_song_data_GPX(const Song &song) {
    REQUIRE(song.name() == "Example File 1");
    REQUIRE(song.artist() == "SonarPractice");
    verify_example_track(song, 1, {64, 59, 55, 50, 45, 40});
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

TEST_CASE("Integration Test: Real GP7 file parsing", "[gpx][gp7][integration]") {
    const std::filesystem::path path = test::test_data_dir() / "testfile.gp";
    if (!std::filesystem::exists(path)) {
        SKIP("testfile.gp not found in test data directory");
    }

    auto result = load_song(path);
    INFO(result.error().message);
    REQUIRE(result);
    verify_song_data_GPX(result.value());
}

TEST_CASE("GTPSongNormalizer sets percussion bank for GM channel 9", "[gtp][normalizer]") {
    Song song;
    Channel channel;
    channel.channel_id = 1;
    channel.program = 25;
    channel.bank = kDefaultBank;
    set_parameter(channel, kGmChannel1Key, "9");
    set_parameter(channel, kGmChannel2Key, "10");
    song.channels.push_back(channel);

    normalize_gtp_channels(song);

    REQUIRE(song.channels.front().is_percussion());
    REQUIRE(song.channels.front().program == 0);
    REQUIRE(song.channels.front().bank == kDefaultPercussionBank);
    REQUIRE(find_parameter(song.channels.front(), kGmChannel1Key)->value == "9");
    REQUIRE(find_parameter(song.channels.front(), kGmChannel2Key)->value == "9");
}