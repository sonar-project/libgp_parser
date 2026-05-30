#include <libgp_parser/track.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace libgp_parser;

TEST_CASE("Track defaults", "[track]") {
    Track track;

    REQUIRE(track.id == 0);
    REQUIRE(track.name.empty());
    REQUIRE(track.tuning_pitches.empty());
    REQUIRE_FALSE(track.color.has_value());
    REQUIRE(track.channel_id == -1);
    REQUIRE_FALSE(track.is_percussion());
}

TEST_CASE("Track percussion channel", "[track]") {
    Track track;
    track.gm_channel1 = kDefaultPercussionChannel;

    REQUIRE(track.is_percussion());
}

TEST_CASE("Track tuning and color", "[track]") {
    Track track;
    constexpr int kE4Pitch = 64;
    constexpr int kB3Pitch = 59;
    constexpr int kG3Pitch = 55;
    constexpr int kD3Pitch = 50;
    constexpr int kA2Pitch = 45;
    constexpr int kE2Pitch = 40;
    track.tuning_pitches = {kE4Pitch, kB3Pitch, kG3Pitch, kD3Pitch, kA2Pitch, kE2Pitch};

    constexpr int kRed = 255;
    constexpr int kGreen = 0;
    constexpr int kBlue = 128;
    track.color = RgbColor{.r = kRed, .g = kGreen, .b = kBlue};

    REQUIRE(track.tuning_pitches.size() == 6);
    REQUIRE(track.color->g == kGreen);
}
