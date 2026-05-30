#include "libgp_parser/gpx_track_reader.hpp"
#include "libgp_parser/gpx_xml.hpp"

#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
#include <string>

namespace libgp_parser {
    namespace {

        using namespace gpx_xml;

        constexpr int ColorNumber255{255};

        pugi::xml_node find_tracks_container(const pugi::xml_document &doc) {
            const pugi::xml_node root = doc.document_element();
            if (!root) {
                return {};
            }
            return direct_child(root, "Tracks");
        }

        void apply_color(Track &track, const std::vector<int> &rgb) {
            if (rgb.size() >= 3) {
                track.color = RgbColor{
                    .r = static_cast<std::uint8_t>(std::clamp(rgb[0], 0, ColorNumber255)),
                    .g = static_cast<std::uint8_t>(std::clamp(rgb[1], 0, ColorNumber255)),
                    .b = static_cast<std::uint8_t>(std::clamp(rgb[2], 0, ColorNumber255)),
                };
            }
        }

        void read_gp6_midi(Track &track, const pugi::xml_node &track_node) {
            const pugi::xml_node gmTrack = direct_child(track_node, "GeneralMidi");
            if (!gmTrack) {
                return;
            }
            track.gm_program = child_int(gmTrack, "Program", track.gm_program);
            const int primary = child_int(gmTrack, "PrimaryChannel", -1);
            const int secondary = child_int(gmTrack, "SecondaryChannel", -1);
            if (primary >= 0) {
                track.gm_channel1 = primary;
            }
            if (secondary >= 0) {
                track.gm_channel2 = secondary;
            }
        }

        void read_gp7_midi(Track &track, const pugi::xml_node &track_node) {
            const pugi::xml_node midi_connection = direct_child(track_node, "MidiConnection");
            int primary = -1;
            int secondary = -1;
            if (midi_connection != nullptr) {
                primary = child_int(midi_connection, "PrimaryChannel", -1);
                secondary = child_int(midi_connection, "SecondaryChannel", -1);
            }

            const pugi::xml_node sounds = direct_child(track_node, "Sounds");
            if (sounds != nullptr) {
                for (pugi::xml_node sound : sounds.children("Sound")) {
                    const pugi::xml_node midi = direct_child(sound, "MIDI");
                    if (midi != nullptr) {
                        track.gm_program = child_int(midi, "Program", track.gm_program);
                    }
                }
            }

            if (primary >= 0 && secondary >= 0) {
                track.gm_channel1 = primary;
                track.gm_channel2 = secondary;
                return;
            }

            bool is_percussion = false;
            const pugi::xml_node instrument = direct_child(track_node, "InstrumentSet");
            if (instrument != nullptr) {
                const std::string name = direct_child_text(instrument, "Name");
                const std::string type = direct_child_text(instrument, "Type");
                auto contains_drum = [](const std::string &my_str) {
                    std::string lower = my_str;
                    std::transform(lower.begin(), lower.end(), lower.begin(),
                                   [](unsigned char my_char) {
                                       return static_cast<char>(std::tolower(my_char));
                                   });
                    return lower.contains("drum");
                };
                is_percussion = contains_drum(name) || contains_drum(type);
            }
            is_percussion = is_percussion && (track.gm_program == 0);
            if (is_percussion) {
                track.gm_channel1 = kDefaultPercussionChannel;
                track.gm_channel2 = kDefaultPercussionChannel;
            }
        }

        void read_properties(Track &track, const pugi::xml_node &properties_parent) {
            if (!properties_parent) {
                return;
            }
            const pugi::xml_node properties = direct_child(properties_parent, "Properties");
            if (!properties) {
                return;
            }
            for (pugi::xml_node property : properties.children("Property")) {
                const std::string name = property.attribute("name").as_string();
                if (name == "Tuning") {
                    track.tuning_pitches = parse_int_list(direct_child_text(property, "Pitches"));
                } else if (name == "CapoFret") {
                    track.capo = child_int(property, "Fret", 0);
                }
            }
        }

        void read_track(Track &track, const pugi::xml_node &track_node, const bool is_gp7) {
            track.id = attribute_int(track_node, "id", track.id);

            std::string name = direct_child_text(track_node, "Name");
            name.erase(std::remove(name.begin(), name.end(), '\n'), name.end());
            const auto trim_start = name.find_first_not_of(" \t\r");
            if (trim_start != std::string::npos) {
                name.erase(0, trim_start);
            }
            const auto trim_end = name.find_last_not_of(" \t\r");
            if (trim_end != std::string::npos) {
                name.erase(trim_end + 1);
            } else {
                name.clear();
            }
            track.name = std::move(name);

            apply_color(track, parse_int_list(direct_child_text(track_node, "Color")));

            if (is_gp7) {
                read_gp7_midi(track, track_node);
                const pugi::xml_node staves = direct_child(track_node, "Staves");
                const pugi::xml_node staff =
                    (staves != nullptr) ? direct_child(staves, "Staff") : pugi::xml_node{};
                read_properties(track, staff);
            } else {
                read_gp6_midi(track, track_node);
                read_properties(track, track_node);
            }
        }

    } // namespace

    ParseResult<std::vector<Track>> parse_gpx_tracks(const std::string_view xml,
                                                     const bool is_gp7) {
        pugi::xml_document doc;
        const pugi::xml_parse_result parsed =
            doc.load_buffer(xml.data(), xml.size(), pugi::parse_default, pugi::encoding_utf8);
        if (!parsed) {
            return ParseResult<std::vector<Track>>::failure(
                {.code = ParseErrorCode::Xml,
                 .message = std::string("XML parse error: ") + parsed.description()});
        }

        const pugi::xml_node tracks_node = find_tracks_container(doc);
        if (!tracks_node) {
            return ParseResult<std::vector<Track>>::success({});
        }

        std::vector<Track> tracks;
        int number = 1;
        for (pugi::xml_node track_node : tracks_node.children("Track")) {
            Track track;
            track.number = number++;
            read_track(track, track_node, is_gp7);
            tracks.push_back(std::move(track));
        }

        return ParseResult<std::vector<Track>>::success(std::move(tracks));
    }

} // namespace libgp_parser
