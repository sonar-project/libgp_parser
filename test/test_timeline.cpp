#include <libgp_parser/timeline.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace libgp_parser;

namespace {

    Track guitar_track() {
        Track track;
        track.tuning_pitches = {64, 59, 55, 50, 45, 40};
        track.capo = 0;
        return track;
    }

    MeasureHeader header_at(const int number, const bool repeat_open = false,
                            const int repeat_close = 0, const int repeat_alternative = 0) {
        MeasureHeader header;
        header.number = number;
        header.start = kQuarterTime + static_cast<long>(number - 1) * header.length();
        header.repeat_open = repeat_open;
        header.repeat_close = repeat_close;
        header.repeat_alternative = repeat_alternative;
        return header;
    }

    std::vector<int> playback_indices(const std::vector<PlaybackMeasure> &playback) {
        std::vector<int> indices;
        indices.reserve(playback.size());
        for (const PlaybackMeasure &step : playback) {
            indices.push_back(step.header_index);
        }
        return indices;
    }

} // namespace

TEST_CASE("midi_pitch is capo plus fret plus open string", "[timeline][pitch]") {
    Track track = guitar_track();
    Note note;
    note.string = 6;
    note.value = 3;
    REQUIRE(midi_pitch(track, note) == 43);

    track.capo = 2;
    REQUIRE(midi_pitch(track, note) == 45);
}

TEST_CASE("midi_pitch uses the same formula for percussion tracks", "[timeline][pitch]") {
    Track track;
    track.gm_channel1 = kDefaultPercussionChannel;
    track.tuning_pitches = {38, 38, 38, 38, 38, 38};
    Note note;
    note.string = 1;
    note.value = 0;
    REQUIRE(track.is_percussion());
    REQUIRE(midi_pitch(track, note) == 38);
}

TEST_CASE("ticks_to_ms matches TGTempo.getTicksInMillis", "[timeline][ms]") {
    REQUIRE(ticks_to_ms(kQuarterTime, 120) == 500);
    REQUIRE(ticks_to_ms(kQuarterTime * 2, 120) == 1000);
    REQUIRE(ticks_to_ms(kQuarterTime, 60) == 1000);
    REQUIRE(ticks_to_ms(kQuarterTime, 0) == 0);
}

TEST_CASE("expand_repeats walks headers in order when there are no repeats", "[timeline][repeat]") {
    Song song;
    song.measure_headers = {header_at(1), header_at(2), header_at(3)};

    const auto playback = expand_repeats(song);
    REQUIRE(playback_indices(playback) == std::vector<int>{0, 1, 2});
    REQUIRE(playback[0].pass == 0);
    REQUIRE(playback[0].repeat_move == 0);
}

TEST_CASE("expand_repeats plays a closed repeat twice", "[timeline][repeat]") {
    Song song;
    song.measure_headers = {header_at(1, true), header_at(2, false, 1)};

    const auto playback = expand_repeats(song);
    REQUIRE(playback_indices(playback) == std::vector<int>{0, 1, 0, 1});
    REQUIRE(playback[0].pass == 0);
    REQUIRE(playback[2].pass == 1);
    REQUIRE(playback[2].repeat_move == 2 * 4 * kQuarterTime);
}

TEST_CASE("expand_repeats honours alternate endings", "[timeline][repeat]") {
    Song song;
    // |: A | [1. B :|] [2. C]
    song.measure_headers = {header_at(1, true), header_at(2, false, 1, 1 << 0),
                            header_at(3, false, 0, 1 << 1)};

    REQUIRE(playback_indices(expand_repeats(song)) == std::vector<int>{0, 1, 0, 2});
}

TEST_CASE("expand_repeats plays consecutive one-bar repeats twice each", "[timeline][repeat]") {
    Song song;
    song.measure_headers = {header_at(1, true, 1), header_at(2, true, 1)};

    const auto playback = expand_repeats(song);
    REQUIRE(playback_indices(playback) == std::vector<int>{0, 0, 1, 1});
    REQUIRE(playback[1].repeat_move == 4 * kQuarterTime);
    REQUIRE(playback[3].repeat_move == 2 * 4 * kQuarterTime);
}

TEST_CASE("expand_repeats ignores the close when the loop is a single opening bar",
          "[timeline][repeat]") {
    Song song;
    song.measure_headers = {header_at(1, true, 1), header_at(2), header_at(3)};

    REQUIRE(playback_indices(expand_repeats(song, 1, 1)) == std::vector<int>{0});
    // Close on bar 1 still fires when the loop continues past that bar.
    REQUIRE(playback_indices(expand_repeats(song, 1, 3)) == std::vector<int>{0, 0, 1, 2});
}
