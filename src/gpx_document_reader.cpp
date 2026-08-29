#include "libgp_parser/gpx_document.hpp"
#include "libgp_parser/gpx_xml.hpp"

#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

namespace libgp_parser {
    namespace {

        using namespace gpx_xml;

        std::optional<int> child_optional_int(const pugi::xml_node &parent, std::string_view name) {
            const pugi::xml_node child = direct_child(parent, name);
            if (!child) {
                return std::nullopt;
            }
            return parse_int_or(child.child_value(), 0);
        }

        std::optional<int> property_float(const pugi::xml_node &property) {
            const pugi::xml_node child = direct_child(property, "Float");
            if (!child) {
                return std::nullopt;
            }
            return parse_int_or(child.child_value(), 0);
        }

        bool property_enabled(const pugi::xml_node &property) {
            return static_cast<bool>(direct_child(property, "Enable"));
        }

        int free_gm_channel(const GpxDocument &document, const GpxTrackData *track_to_check) {
            int gm_channel = 0;
            bool used = false;
            do {
                ++gm_channel;
                used = track_to_check != nullptr && (track_to_check->gm_channel1 == gm_channel ||
                                                     track_to_check->gm_channel2 == gm_channel);
                for (const GpxTrackData &track : document.tracks) {
                    used = used || track.gm_channel1 == gm_channel || track.gm_channel2 == gm_channel;
                }
            } while (used || gm_channel == kDefaultPercussionChannel);
            return gm_channel;
        }

        void read_gp6_midi(GpxTrackData &track, const pugi::xml_node &track_node,
                           const GpxDocument &document) {
            const pugi::xml_node gm = direct_child(track_node, "GeneralMidi");
            if (!gm) {
                return;
            }
            track.gm_program = child_int(gm, "Program", track.gm_program);
            track.gm_channel1 = child_int(gm, "PrimaryChannel", free_gm_channel(document, nullptr));
            track.gm_channel2 = child_int(gm, "SecondaryChannel", free_gm_channel(document, &track));
        }

        void read_gp7_midi(GpxTrackData &track, const pugi::xml_node &track_node,
                           GpxDocument &document) {
            int primary = -1;
            int secondary = -1;
            const pugi::xml_node midi_connection = direct_child(track_node, "MidiConnection");
            if (midi_connection) {
                primary = child_int(midi_connection, "PrimaryChannel", -1);
                secondary = child_int(midi_connection, "SecondaryChannel", -1);
            }
            const pugi::xml_node sounds = direct_child(track_node, "Sounds");
            if (sounds) {
                for (pugi::xml_node sound : sounds.children("Sound")) {
                    const pugi::xml_node midi = direct_child(sound, "MIDI");
                    if (midi) {
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
            if (instrument) {
                auto contains_drum = [](std::string text) {
                    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
                        return static_cast<char>(std::tolower(ch));
                    });
                    return text.contains("drum");
                };
                is_percussion = contains_drum(direct_child_text(instrument, "Name")) ||
                                contains_drum(direct_child_text(instrument, "Type"));
            }
            is_percussion = is_percussion && track.gm_program == 0;
            if (is_percussion) {
                track.gm_channel1 = kDefaultPercussionChannel;
                track.gm_channel2 = kDefaultPercussionChannel;
            } else {
                track.gm_channel1 = free_gm_channel(document, nullptr);
                track.gm_channel2 = free_gm_channel(document, &track);
            }
        }

        void read_chords(GpxDocument &document, const pugi::xml_node &properties) {
            if (!properties) {
                return;
            }
            for (pugi::xml_node property : properties.children("Property")) {
                if (attribute_string(property, "name") != "DiagramCollection") {
                    continue;
                }
                const pugi::xml_node items = direct_child(property, "Items");
                if (!items) {
                    continue;
                }
                for (pugi::xml_node item : items.children("Item")) {
                    const pugi::xml_node diagram = direct_child(item, "Diagram");
                    if (!diagram) {
                        continue;
                    }
                    GpxChord chord;
                    chord.id = attribute_int(item, "id");
                    chord.name = attribute_string(item, "name");
                    chord.string_count = attribute_int(diagram, "stringCount");
                    chord.base_fret = attribute_int(diagram, "baseFret");
                    chord.frets.assign(static_cast<std::size_t>(std::max(chord.string_count, 0)),
                                       std::nullopt);
                    for (pugi::xml_node fret : diagram.children("Fret")) {
                        const int string = attribute_int(fret, "string");
                        if (string >= 0 && string < chord.string_count) {
                            chord.frets[static_cast<std::size_t>(string)] =
                                attribute_int(fret, "fret");
                        }
                    }
                    document.chords.push_back(std::move(chord));
                }
            }
        }

        pugi::xml_node properties_parent(const pugi::xml_node &track_node, const bool is_gp7) {
            if (!is_gp7) {
                return track_node;
            }
            const pugi::xml_node staves = direct_child(track_node, "Staves");
            if (!staves) {
                return {};
            }
            return direct_child(staves, "Staff");
        }

        void read_track_properties(GpxTrackData &track, const pugi::xml_node &properties) {
            if (!properties) {
                return;
            }
            for (pugi::xml_node property : properties.children("Property")) {
                const std::string name = attribute_string(property, "name");
                if (name == "Tuning") {
                    track.tuning_pitches = parse_int_list(direct_child_text(property, "Pitches"));
                } else if (name == "CapoFret") {
                    track.capo = child_int(property, "Fret", 0);
                }
            }
        }

        void read_score(GpxDocument &document, const pugi::xml_node &root) {
            const pugi::xml_node score = direct_child(root, "Score");
            if (!score) {
                return;
            }
            document.score.title = direct_child_text(score, "Title");
            document.score.subtitle = direct_child_text(score, "SubTitle");
            document.score.artist = direct_child_text(score, "Artist");
            document.score.album = direct_child_text(score, "Album");
            document.score.words = direct_child_text(score, "Words");
            document.score.music = direct_child_text(score, "Music");
            document.score.words_and_music = direct_child_text(score, "WordsAndMusic");
            document.score.copyright = direct_child_text(score, "Copyright");
            document.score.tabber = direct_child_text(score, "Tabber");
            document.score.instructions = direct_child_text(score, "Instructions");
            document.score.notices = direct_child_text(score, "Notices");
        }

        void read_automations(GpxDocument &document, const pugi::xml_node &root) {
            const pugi::xml_node master = direct_child(root, "MasterTrack");
            if (!master) {
                return;
            }
            const pugi::xml_node automations = direct_child(master, "Automations");
            if (!automations) {
                return;
            }
            for (pugi::xml_node node : automations.children("Automation")) {
                GpxAutomation automation;
                automation.type = direct_child_text(node, "Type");
                automation.bar_id = child_int(node, "Bar", 0);
                automation.value = parse_int_list(direct_child_text(node, "Value"));
                automation.linear = child_bool(node, "Linear");
                automation.position = child_int(node, "Position", 0);
                automation.visible = child_bool(node, "Visible");
                document.automations.push_back(std::move(automation));
            }
        }

        void read_tracks(GpxDocument &document, const pugi::xml_node &root, const bool is_gp7) {
            const pugi::xml_node tracks = direct_child(root, "Tracks");
            if (!tracks) {
                return;
            }
            for (pugi::xml_node track_node : tracks.children("Track")) {
                GpxTrackData track;
                track.id = attribute_int(track_node, "id");
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
                track.color = parse_int_list(direct_child_text(track_node, "Color"));

                if (is_gp7) {
                    read_gp7_midi(track, track_node, document);
                } else {
                    read_gp6_midi(track, track_node, document);
                }

                const pugi::xml_node parent = properties_parent(track_node, is_gp7);
                const pugi::xml_node properties = parent ? direct_child(parent, "Properties") : parent;
                read_track_properties(track, properties);
                document.tracks.push_back(std::move(track));
                read_chords(document, properties);
            }
        }

        void read_master_bars(GpxDocument &document, const pugi::xml_node &root) {
            const pugi::xml_node master_bars = direct_child(root, "MasterBars");
            if (!master_bars) {
                return;
            }
            for (pugi::xml_node node : master_bars.children("MasterBar")) {
                GpxMasterBar bar;
                bar.bar_ids = child_int_list(node, "Bars");
                bar.time = child_int_list(node, "Time", "/");
                bar.triplet_feel = direct_child_text(node, "TripletFeel");
                const pugi::xml_node repeat = direct_child(node, "Repeat");
                if (repeat) {
                    bar.repeat_start = attribute_bool(repeat, "start");
                    if (attribute_bool(repeat, "end")) {
                        bar.repeat_count = attribute_int(repeat, "count") - 1;
                    }
                }
                const pugi::xml_node key = direct_child(node, "Key");
                if (key) {
                    bar.accidental_count = child_int(key, "AccidentalCount", 0);
                    bar.mode = direct_child_text(key, "Mode");
                }
                bar.alternate_endings = child_int_list(node, "AlternateEndings");
                const pugi::xml_node section = direct_child(node, "Section");
                if (section) {
                    std::string text = direct_child_text(section, "Text");
                    text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
                    const auto start = text.find_first_not_of(" \t\r");
                    if (start != std::string::npos) {
                        text.erase(0, start);
                    }
                    const auto end = text.find_last_not_of(" \t\r");
                    if (end != std::string::npos) {
                        text.erase(end + 1);
                    } else {
                        text.clear();
                    }
                    bar.marker_text = std::move(text);
                }
                document.master_bars.push_back(std::move(bar));
            }
        }

        void read_bars(GpxDocument &document, const pugi::xml_node &root) {
            const pugi::xml_node bars = direct_child(root, "Bars");
            if (!bars) {
                return;
            }
            for (pugi::xml_node node : bars.children("Bar")) {
                GpxBar bar;
                bar.id = attribute_int(node, "id");
                bar.voice_ids = child_int_list(node, "Voices");
                bar.clef = direct_child_text(node, "Clef");
                bar.simile_mark = direct_child_text(node, "SimileMark");
                document.bars.push_back(std::move(bar));
            }
        }

        void read_voices(GpxDocument &document, const pugi::xml_node &root) {
            const pugi::xml_node voices = direct_child(root, "Voices");
            if (!voices) {
                return;
            }
            for (pugi::xml_node node : voices.children("Voice")) {
                GpxVoice voice;
                voice.id = attribute_int(node, "id");
                voice.beat_ids = child_int_list(node, "Beats");
                document.voices.push_back(std::move(voice));
            }
        }

        void read_beats(GpxDocument &document, const pugi::xml_node &root) {
            const pugi::xml_node beats = direct_child(root, "Beats");
            if (!beats) {
                return;
            }
            for (pugi::xml_node node : beats.children("Beat")) {
                GpxBeat beat;
                beat.id = attribute_int(node, "id");
                beat.dynamic = direct_child_text(node, "Dynamic");
                beat.text = direct_child_text(node, "FreeText");
                const pugi::xml_node rhythm = direct_child(node, "Rhythm");
                beat.rhythm_id = rhythm ? attribute_int(rhythm, "ref") : 0;
                beat.tremolo = child_int_list(node, "Tremolo", "/");
                beat.note_ids = child_int_list(node, "Notes");
                beat.chord_id = child_optional_int(node, "Chord");
                beat.fadding = direct_child_text(node, "Fadding");
                beat.grace_notes = direct_child_text(node, "GraceNotes");

                const pugi::xml_node properties = direct_child(node, "Properties");
                if (properties) {
                    for (pugi::xml_node property : properties.children("Property")) {
                        const std::string name = attribute_string(property, "name");
                        if (name == "PickStroke") {
                            beat.pick_stroke = direct_child_text(property, "Direction");
                        } else if (name == "WhammyBar") {
                            beat.whammy_bar_enabled = property_enabled(property);
                        } else if (name == "WhammyBarOriginValue") {
                            beat.whammy_bar_origin_value = property_float(property);
                        } else if (name == "WhammyBarMiddleValue") {
                            beat.whammy_bar_middle_value = property_float(property);
                        } else if (name == "WhammyBarDestinationValue") {
                            beat.whammy_bar_destination_value = property_float(property);
                        } else if (name == "WhammyBarOriginOffset") {
                            beat.whammy_bar_origin_offset = property_float(property);
                        } else if (name == "WhammyBarMiddleOffset1") {
                            beat.whammy_bar_middle_offset1 = property_float(property);
                        } else if (name == "WhammyBarMiddleOffset2") {
                            beat.whammy_bar_middle_offset2 = property_float(property);
                        } else if (name == "WhammyBarDestinationOffset") {
                            beat.whammy_bar_destination_offset = property_float(property);
                        } else if (name == "Brush") {
                            beat.brush = direct_child_text(property, "Direction");
                        } else if (name == "Slapped") {
                            beat.slapped = property_enabled(property);
                        } else if (name == "Popped") {
                            beat.popped = property_enabled(property);
                        }
                    }
                }
                document.beats.push_back(std::move(beat));
            }
        }

        void read_notes(GpxDocument &document, const pugi::xml_node &root) {
            const pugi::xml_node notes = direct_child(root, "Notes");
            if (!notes) {
                return;
            }
            for (pugi::xml_node node : notes.children("Note")) {
                GpxNote note;
                note.id = attribute_int(node, "id");
                const pugi::xml_node tie = direct_child(node, "Tie");
                note.tie_destination =
                    tie && attribute_string(tie, "destination") == "true";
                const std::string ghost = direct_child_text(node, "AntiAccent");
                note.ghost = ghost == "Normal";
                note.accent = child_int(node, "Accent", 0);
                note.trill = child_int(node, "Trill", 0);
                note.let_ring = static_cast<bool>(direct_child(node, "LetRing"));
                note.vibrato = static_cast<bool>(direct_child(node, "Vibrato"));

                const pugi::xml_node properties = direct_child(node, "Properties");
                if (properties) {
                    for (pugi::xml_node property : properties.children("Property")) {
                        const std::string name = attribute_string(property, "name");
                        if (name == "String") {
                            note.string = child_int(property, "String", -1);
                        } else if (name == "Fret") {
                            note.fret = child_int(property, "Fret", -1);
                        } else if (name == "Midi") {
                            note.midi_number = child_int(property, "Number", -1);
                        } else if (name == "Tone") {
                            note.tone = child_int(property, "Step", -1);
                        } else if (name == "Octave") {
                            note.octave = child_int(property, "Number", -1);
                        } else if (name == "Element") {
                            note.element = child_int(property, "Element", -1);
                        } else if (name == "Variation") {
                            note.variation = child_int(property, "Variation", -1);
                        } else if (name == "Muted") {
                            note.muted_enabled = property_enabled(property);
                        } else if (name == "PalmMuted") {
                            note.palm_muted_enabled = property_enabled(property);
                        } else if (name == "Slide") {
                            note.slide = true;
                            note.slide_flags = child_int(property, "Flags", 0);
                        } else if (name == "Tapped") {
                            note.tapped = property_enabled(property);
                        } else if (name == "Bended") {
                            note.bend_enabled = property_enabled(property);
                        } else if (name == "BendOriginValue") {
                            note.bend_origin_value = property_float(property);
                        } else if (name == "BendMiddleValue") {
                            note.bend_middle_value = property_float(property);
                        } else if (name == "BendDestinationValue") {
                            note.bend_destination_value = property_float(property);
                        } else if (name == "BendOriginOffset") {
                            note.bend_origin_offset = property_float(property);
                        } else if (name == "BendMiddleOffset1") {
                            note.bend_middle_offset1 = property_float(property);
                        } else if (name == "BendMiddleOffset2") {
                            note.bend_middle_offset2 = property_float(property);
                        } else if (name == "BendDestinationOffset") {
                            note.bend_destination_offset = property_float(property);
                        } else if (name == "HopoOrigin") {
                            note.hammer = true;
                        } else if (name == "HarmonicFret") {
                            note.harmonic_fret = child_int(property, "HFret", -1);
                        } else if (name == "HarmonicType") {
                            note.harmonic_type = direct_child_text(property, "HType");
                        }
                    }
                }

                const pugi::xml_node x_properties = direct_child(node, "XProperties");
                if (x_properties) {
                    for (pugi::xml_node x_property : x_properties.children("XProperty")) {
                        if (attribute_string(x_property, "id") == "688062467") {
                            note.trill_duration = child_int(x_property, "Int", 0);
                        }
                    }
                }
                document.notes.push_back(std::move(note));
            }
        }

        void read_rhythms(GpxDocument &document, const pugi::xml_node &root) {
            const pugi::xml_node rhythms = direct_child(root, "Rhythms");
            if (!rhythms) {
                return;
            }
            for (pugi::xml_node node : rhythms.children("Rhythm")) {
                GpxRhythm rhythm;
                rhythm.id = attribute_int(node, "id");
                rhythm.note_value = direct_child_text(node, "NoteValue");
                const pugi::xml_node tuplet = direct_child(node, "PrimaryTuplet");
                rhythm.primary_tuplet_den = tuplet ? attribute_int(tuplet, "den", 1) : 1;
                rhythm.primary_tuplet_num = tuplet ? attribute_int(tuplet, "num", 1) : 1;
                const pugi::xml_node dots = direct_child(node, "AugmentationDot");
                rhythm.augmentation_dot_count = dots ? attribute_int(dots, "count", 0) : 0;
                document.rhythms.push_back(std::move(rhythm));
            }
        }

    } // namespace

    ParseResult<GpxDocument> parse_gpx_document(const std::string_view xml, const bool is_gp7) {
        pugi::xml_document xml_doc;
        const pugi::xml_parse_result parsed =
            xml_doc.load_buffer(xml.data(), xml.size(), pugi::parse_default, pugi::encoding_utf8);
        if (!parsed) {
            return ParseResult<GpxDocument>::failure(
                {.code = ParseErrorCode::Xml,
                 .message = std::string("XML parse error: ") + parsed.description()});
        }

        GpxDocument document;
        const pugi::xml_node root = xml_doc.document_element();
        if (!root) {
            return ParseResult<GpxDocument>::success(std::move(document));
        }
        read_score(document, root);
        read_automations(document, root);
        read_tracks(document, root, is_gp7);
        read_master_bars(document, root);
        read_bars(document, root);
        read_voices(document, root);
        read_beats(document, root);
        read_notes(document, root);
        read_rhythms(document, root);
        return ParseResult<GpxDocument>::success(std::move(document));
    }

} // namespace libgp_parser
