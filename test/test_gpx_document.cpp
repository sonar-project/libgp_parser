#include <libgp_parser/gpx_document.hpp>

#include "test_helpers.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace libgp_parser;

TEST_CASE("GPX document reads master bars, beats, and notes", "[gpx][document]") {
    const std::string xml = test::read_fixture("notes_score.gpif");
    auto document = parse_gpx_document(xml, false);
    REQUIRE(document);
    REQUIRE(document.value().master_bars.size() == 1);
    REQUIRE(document.value().bars.size() == 1);
    REQUIRE(document.value().beats.size() == 1);
    REQUIRE(document.value().notes.size() == 1);
    REQUIRE(document.value().notes.front().fret == 3);
    REQUIRE(document.value().rhythms.front().note_value == "Quarter");
    REQUIRE(document.value().automations.size() == 1);
    REQUIRE(document.value().automations.front().type == "Tempo");
    REQUIRE(document.value().automations.front().value.front() == 120);
    REQUIRE(document.value().master_bars.front().repeat_start);
}

TEST_CASE("GPX mapper produces measures and notes", "[gpx][parser]") {
    const std::string xml = test::read_fixture("notes_score.gpif");
    auto document = parse_gpx_document(xml, false);
    REQUIRE(document);

    const Song song = map_gpx_document(document.value());
    REQUIRE(song.name() == "Note Song");
    REQUIRE(song.measure_count() == 1);
    REQUIRE(song.track_count() == 1);
    REQUIRE(song.tracks.front().measures.size() == 1);
    REQUIRE(song.tracks.front().string_count() == 6);
    REQUIRE(song.measure_headers.front().repeat_open);
    REQUIRE(song.measure_headers.front().time_signature.numerator == 4);
    REQUIRE(song.measure_headers.front().time_signature.denominator.value == DurationValue::Quarter);

    const Measure &measure = song.tracks.front().measures.front();
    REQUIRE_FALSE(measure.beats.empty());
    const Beat &beat = measure.beats.front();
    REQUIRE_FALSE(beat.voice(0).empty);
    REQUIRE(beat.voice(0).notes.size() == 1);
    REQUIRE(beat.voice(0).notes.front().value == 3);
    REQUIRE(beat.voice(0).notes.front().string == 6);
    REQUIRE(beat.voice(0).duration.value == DurationValue::Quarter);
    REQUIRE(beat.stroke.direction == StrokeDirection::Down);
}

TEST_CASE("GPX mapper maps bend, slide, hammer, chord, and marker", "[gpx][parser][effects]") {
    const std::string xml = test::read_fixture("effects_score.gpif");
    auto document = parse_gpx_document(xml, false);
    REQUIRE(document);
    REQUIRE(document.value().chords.size() == 1);
    REQUIRE(document.value().chords.front().name == "Am");
    REQUIRE(document.value().notes.front().bend_enabled);
    REQUIRE(document.value().notes.front().hammer);
    REQUIRE(document.value().notes.front().slide);
    REQUIRE(document.value().master_bars.front().marker_text == "Intro");

    const Song song = map_gpx_document(document.value());
    REQUIRE(song.measure_headers.front().repeat_open);
    REQUIRE(song.measure_headers.front().repeat_close == 1);
    REQUIRE(song.measure_headers.front().has_marker());
    REQUIRE(song.measure_headers.front().marker->title == "Intro");

    const Beat &beat = song.tracks.front().measures.front().beats.front();
    REQUIRE(beat.chord.has_value());
    REQUIRE(beat.chord->name == "Am");
    REQUIRE(beat.voice(0).notes.size() == 1);
    const Note &note = beat.voice(0).notes.front();
    REQUIRE(note.effect.hammer);
    REQUIRE(note.effect.slide);
    REQUIRE(note.effect.has_bend());
    REQUIRE(note.effect.bend->points.front().value == 0);
    REQUIRE(note.effect.bend->points.back().position == EffectBend::MaxPositionLength);
    REQUIRE(note.effect.bend->points.back().value == 4);
}

TEST_CASE("GPX mapper applies mid-song tempo automations to later headers", "[gpx][parser][tempo]") {
    const std::string xml = test::read_fixture("tempo_change_score.gpif");
    auto document = parse_gpx_document(xml, false);
    REQUIRE(document);

    const Song song = map_gpx_document(document.value());
    REQUIRE(song.measure_count() == 2);
    REQUIRE(song.measure_headers[0].tempo.quarter_bpm == 120);
    REQUIRE(song.measure_headers[1].tempo.quarter_bpm == 90);
    REQUIRE(song.tempo_bpm == 120);
}

TEST_CASE("GPX mapper keeps two voices on the same beat", "[gpx][parser][voices]") {
    const std::string xml = test::read_fixture("two_voices_score.gpif");
    auto document = parse_gpx_document(xml, false);
    REQUIRE(document);

    const Song song = map_gpx_document(document.value());
    REQUIRE(song.tracks.front().measures.size() == 1);
    const Beat &beat = song.tracks.front().measures.front().beats.front();
    REQUIRE_FALSE(beat.voice(0).empty);
    REQUIRE_FALSE(beat.voice(1).empty);
    REQUIRE(beat.voice(0).notes.front().string == 6);
    REQUIRE(beat.voice(0).notes.front().value == 3);
    REQUIRE(beat.voice(0).duration.value == DurationValue::Quarter);
    REQUIRE(beat.voice(1).notes.front().string == 5);
    REQUIRE(beat.voice(1).notes.front().value == 5);
    REQUIRE(beat.voice(1).duration.value == DurationValue::Eighth);
}

TEST_CASE("GPX mapper copies notes from a Simple simile mark", "[gpx][parser][simile]") {
    const std::string xml = test::read_fixture("simile_score.gpif");
    auto document = parse_gpx_document(xml, false);
    REQUIRE(document);
    REQUIRE(document.value().bars.size() == 2);
    REQUIRE(document.value().bars[1].simile_mark == "Simple");

    const Song song = map_gpx_document(document.value());
    REQUIRE(song.measure_count() == 2);
    REQUIRE(song.tracks.front().measures.size() == 2);
    const Note &first = song.tracks.front().measures[0].beats.front().voice(0).notes.front();
    const Note &copied = song.tracks.front().measures[1].beats.front().voice(0).notes.front();
    REQUIRE(first.value == 7);
    REQUIRE(copied.value == 7);
    REQUIRE(copied.string == first.string);
}
