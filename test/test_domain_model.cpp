#include <libgp_parser/beat.hpp>
#include <libgp_parser/channel.hpp>
#include <libgp_parser/duration.hpp>
#include <libgp_parser/effects.hpp>
#include <libgp_parser/measure.hpp>
#include <libgp_parser/song.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace libgp_parser;

TEST_CASE("Duration quarter time matches TuxGuitar", "[duration]") {
    Duration duration;
    REQUIRE(duration.value == DurationValue::Quarter);
    REQUIRE(duration.time() == kQuarterTime);
}

TEST_CASE("Duration dotted eighth", "[duration]") {
    Duration duration;
    duration.value = DurationValue::Eighth;
    duration.dotted = true;
    REQUIRE(duration.time() == (kQuarterTime / 2) + (kQuarterTime / 4));
}

TEST_CASE("Duration triplet eighth", "[duration]") {
    Duration duration;
    duration.value = DurationValue::Eighth;
    duration.division.enters = 3;
    duration.division.times = 2;
    REQUIRE(duration.time() == (kQuarterTime / 2) * 2 / 3);
}

TEST_CASE("MeasureHeader length is numerator times denominator ticks", "[measure]") {
    MeasureHeader header;
    header.time_signature.numerator = 3;
    header.time_signature.denominator.value = DurationValue::Quarter;
    REQUIRE(header.length() == 3 * kQuarterTime);
}

TEST_CASE("Beat starts with two empty voices", "[beat]") {
    Beat beat;
    REQUIRE(beat.voices.size() == kMaxVoices);
    REQUIRE(beat.voice(0).empty);
    REQUIRE(beat.voice(1).empty);

    Note note;
    note.string = 1;
    note.value = 5;
    beat.voice(0).add_note(note);
    REQUIRE_FALSE(beat.voice(0).empty);
    REQUIRE(beat.voice(0).notes.size() == 1);
}

TEST_CASE("Song holds measure headers and channels", "[song]") {
    Song song;
    song.measure_headers.push_back(MeasureHeader{.number = 1});
    song.channels.push_back(Channel{.channel_id = 1, .program = 25});
    song.tracks.push_back(Track{.number = 1, .name = "Guitar"});
    song.tracks[0].measures.push_back(Measure{.header_index = 0});

    REQUIRE(song.measure_count() == 1);
    REQUIRE(song.header_for(song.tracks[0].measures[0]).number == 1);
    REQUIRE_FALSE(song.channels[0].is_percussion());
}

TEST_CASE("Percussion channel uses bank 128", "[channel]") {
    Channel channel;
    channel.bank = kDefaultPercussionBank;
    REQUIRE(channel.is_percussion());
}
