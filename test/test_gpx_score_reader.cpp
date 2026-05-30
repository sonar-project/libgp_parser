#include <libgp_parser/gpx_score_reader.hpp>

#include "test_helpers.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace libgp_parser;

void verify_song_title_and_subtitle(const SongMetadata &mdata) {
    REQUIRE(mdata.title == "Test Song");
    REQUIRE(mdata.subtitle == "Subtitle");
}

void verify_song_artist_and_album(const SongMetadata &mdata) {
    REQUIRE(mdata.artist == "Test Artist");
    REQUIRE(mdata.album == "Test Album");
}

void verify_song_words_and_music(const SongMetadata &mdata) {
    REQUIRE(mdata.words == "Lyrics by");
    REQUIRE(mdata.music == "Music by");
    REQUIRE(mdata.words_and_music == "Words And Music");
}

void verify_song_restdata(const SongMetadata &mdata) {
    REQUIRE(mdata.copyright == "(C) 2026");
    REQUIRE(mdata.tabber == "Tabber Name");
    REQUIRE(mdata.instructions == "Play loud");
    REQUIRE(mdata.notices == "Some notice");
}

void verify_score_metadata(const SongMetadata &mdata) {
    verify_song_title_and_subtitle(mdata);
    verify_song_artist_and_album(mdata);
    verify_song_words_and_music(mdata);
    verify_song_restdata(mdata);
}

TEST_CASE("parse_gpx_score_metadata reads Score element", "[gpx][score]") {
    const std::string xml = test::read_fixture("minimal_score.gpif");
    auto result = parse_gpx_score_metadata(xml);

    REQUIRE(result);
    const SongMetadata &mdata = result.value();
    verify_score_metadata(mdata);
}

TEST_CASE("parse_gpx_score_metadata ignores MasterTrack", "[gpx][score]") {
    const std::string xml = test::read_fixture("minimal_score.gpif");
    auto result = parse_gpx_score_metadata(xml);
    REQUIRE(result);
    REQUIRE(result.value().title == "Test Song");
}

TEST_CASE("parse_gpx_score_metadata rejects invalid XML", "[gpx][score]") {
    auto result = parse_gpx_score_metadata("<not-xml");
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == ParseErrorCode::Xml);
}

TEST_CASE("parse_gpx_score_metadata missing Score", "[gpx][score]") {
    auto result = parse_gpx_score_metadata("<GPIF></GPIF>");
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == ParseErrorCode::NotFound);
}
