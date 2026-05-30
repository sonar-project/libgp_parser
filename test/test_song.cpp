#include <libgp_parser/song.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace libgp_parser;

TEST_CASE("Song defaults", "[song]") {
    Song song;

    REQUIRE(song.metadata.title.empty());
    REQUIRE(song.track_count() == 0);
    REQUIRE(song.name().empty());
}

void verify_song_metadata(const Song &song, const std::string &title, const std::string &artist) {
    REQUIRE(song.metadata.title == title);
    REQUIRE(song.metadata.artist == artist);
}

TEST_CASE("Song metadata maps to TGSong-style accessors", "[song]") {
    Song song;
    song.metadata.title = "Test Piece";
    song.metadata.artist = "Artist";
    song.metadata.album = "Album";
    song.metadata.words_and_music = "Composer";
    song.metadata.copyright = "(C) 2026";
    song.metadata.tabber = "Tabber";
    song.metadata.notices = "Notice";

    verify_song_metadata(song, "Test Piece", "Artist");

    REQUIRE(song.album() == "Album");
    REQUIRE(song.author() == "Composer");
    REQUIRE(song.copyright() == "(C) 2026");
    REQUIRE(song.writer() == "Tabber");
    REQUIRE(song.comments() == "Notice");
}

TEST_CASE("Song holds tracks", "[song]") {
    Song song;
    song.tracks.push_back(Track{.id = 1, .number = 1, .name = "Guitar"});
    song.tracks.push_back(Track{.id = 2, .number = 2, .name = "Bass"});

    REQUIRE(song.track_count() == 2);
    REQUIRE(song.tracks[0].name == "Guitar");
}
