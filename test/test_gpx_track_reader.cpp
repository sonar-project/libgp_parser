#include <libgp_parser/gpx_track_reader.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace libgp_parser;

const std::string example_gpx_xml = R"(<?xml version="1.0" encoding="utf-8"?>
<GPIF>
  <Tracks>
    <Track id="0">
      <Name>Guitar</Name>
      <Color>255 0 128</Color>
      <GeneralMidi>
        <Program>25</Program>
        <PrimaryChannel>1</PrimaryChannel>
        <SecondaryChannel>2</SecondaryChannel>
      </GeneralMidi>
      <Properties>
        <Property name="Tuning"><Pitches>64 59 55 50 45 40</Pitches></Property>
        <Property name="CapoFret"><Fret>2</Fret></Property>
      </Properties>
    </Track>
    <Track id="1">
      <Name>Drums</Name>
      <GeneralMidi>
        <Program>0</Program>
        <PrimaryChannel>9</PrimaryChannel>
        <SecondaryChannel>9</SecondaryChannel>
      </GeneralMidi>
    </Track>
  </Tracks>
</GPIF>)";

void verify_xml_name_id_number(const Track &track, const std::string &expected_name,
                               int expected_id, int expected_number) {
    REQUIRE(track.name == expected_name);
    REQUIRE(track.id == expected_id);
    REQUIRE(track.number == expected_number);
}

void verify_xml_gm_program_channel(const Track &track, int expected_program,
                                   int expected_primary_channel) {
    REQUIRE(track.gm_program == expected_program);
    REQUIRE(track.gm_channel1 == expected_primary_channel);
}

void verify_xml_color(const Track &track) { REQUIRE(track.color.has_value()); }

TEST_CASE("parse_gpx_tracks reads Track elements", "[gpx][tracks]") {
    const std::string xml = example_gpx_xml;

    auto result = parse_gpx_tracks(xml, false);
    REQUIRE(result);
    REQUIRE(result.value().size() == 2);

    const Track &guitar = result.value()[0];
    verify_xml_name_id_number(guitar, "Guitar", 0, 1);

    constexpr int expected_program = 25;
    constexpr int expected_primary_channel = 1;

    verify_xml_gm_program_channel(guitar, expected_program, expected_primary_channel);

    REQUIRE(guitar.tuning_pitches.size() == 6);
    REQUIRE(guitar.capo == 2);
    verify_xml_color(guitar);

    const Track &drums = result.value()[1];
    REQUIRE(drums.is_percussion());
}
