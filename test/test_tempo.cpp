#include <libgp_parser/gpx_tempo_reader.hpp>
#include <libgp_parser/tempo.hpp>

#include "test_helpers.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace libgp_parser;

TEST_CASE("apply_gpx_tempo_modifier", "[tempo]") {
  REQUIRE(apply_gpx_tempo_modifier(120, 2) == 120);
  REQUIRE(apply_gpx_tempo_modifier(120, 1) == 60);
  REQUIRE(apply_gpx_tempo_modifier(120, 3) == 180);
  REQUIRE(apply_gpx_tempo_modifier(120, 4) == 240);
  REQUIRE(apply_gpx_tempo_modifier(100, 5) == 300);
}

TEST_CASE("parse_gpx_initial_tempo", "[tempo][gpx]") {
  const std::string xml = test::read_fixture("minimal_score.gpif");
  auto result = parse_gpx_initial_tempo(xml);
  REQUIRE(result);
  REQUIRE(result.value() == 120);
}

TEST_CASE("parse_gpx_initial_tempo gp6 float value", "[tempo][gpx]") {
  const std::string xml = R"(<?xml version="1.0"?>
<GPIF>
  <MasterTrack>
    <Automations>
      <Automation>
        <Type>Tempo</Type>
        <Bar>0</Bar>
        <Value>120.000000</Value>
      </Automation>
    </Automations>
  </MasterTrack>
</GPIF>)";
  auto result = parse_gpx_initial_tempo(xml);
  REQUIRE(result);
  REQUIRE(result.value() == 120);
}

TEST_CASE("parse_gpx_initial_tempo with modifier", "[tempo][gpx]") {
  const std::string xml = R"(<?xml version="1.0"?>
<GPIF>
  <MasterTrack>
    <Automations>
      <Automation>
        <Type>Tempo</Type>
        <Bar>0</Bar>
        <Value>100 1</Value>
      </Automation>
    </Automations>
  </MasterTrack>
</GPIF>)";
  auto result = parse_gpx_initial_tempo(xml);
  REQUIRE(result);
  REQUIRE(result.value() == 50);
}
