#pragma once

#include "libgp_parser/error.hpp"
#include "libgp_parser/song.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace libgp_parser {

    /// Intermediate GPX XML model (GPXDocument + score types).
    /// @par TuxGuitar source common/TuxGuitar-gpx/.../score/GPX*.java.

    struct GpxAutomation {
        std::string type;
        int bar_id{0};
        int position{0};
        bool linear{false};
        bool visible{false};
        std::vector<int> value;
    };

    struct GpxTrackData {
        int id{0};
        std::string name;
        std::vector<int> color;
        std::vector<int> tuning_pitches;
        int capo{0};
        int gm_program{0};
        int gm_channel1{0};
        int gm_channel2{0};
    };

    struct GpxMasterBar {
        std::vector<int> bar_ids;
        std::vector<int> time;
        int repeat_count{0};
        bool repeat_start{false};
        int accidental_count{0};
        std::string mode;
        std::string triplet_feel;
        std::vector<int> alternate_endings;
        std::string marker_text;
    };

    struct GpxBar {
        int id{-1};
        std::vector<int> voice_ids;
        std::string clef;
        std::string simile_mark;
    };

    struct GpxVoice {
        int id{-1};
        std::vector<int> beat_ids;
    };

    struct GpxBeat {
        int id{-1};
        int rhythm_id{0};
        std::vector<int> note_ids;
        std::string dynamic;
        bool slapped{false};
        bool popped{false};
        std::string brush;
        std::string pick_stroke;
        std::vector<int> tremolo;
        std::string fadding;
        std::string text;
        std::optional<int> chord_id;
        std::string grace_notes;
        bool whammy_bar_enabled{false};
        std::optional<int> whammy_bar_origin_value;
        std::optional<int> whammy_bar_middle_value;
        std::optional<int> whammy_bar_destination_value;
        std::optional<int> whammy_bar_origin_offset;
        std::optional<int> whammy_bar_middle_offset1;
        std::optional<int> whammy_bar_middle_offset2;
        std::optional<int> whammy_bar_destination_offset;
    };

    struct GpxNote {
        int id{-1};
        int fret{-1};
        int string{-1};
        int tone{-1};
        int octave{-1};
        int element{-1};
        int variation{-1};
        int midi_number{-1};
        int trill{0};
        int trill_duration{0};
        bool bend_enabled{false};
        std::optional<int> bend_origin_value;
        std::optional<int> bend_middle_value;
        std::optional<int> bend_destination_value;
        std::optional<int> bend_origin_offset;
        std::optional<int> bend_middle_offset1;
        std::optional<int> bend_middle_offset2;
        std::optional<int> bend_destination_offset;
        bool hammer{false};
        bool ghost{false};
        bool slide{false};
        int slide_flags{0};
        bool vibrato{false};
        bool let_ring{false};
        int accent{0};
        bool tapped{false};
        bool tie_destination{false};
        bool muted_enabled{false};
        bool palm_muted_enabled{false};
        int harmonic_fret{-1};
        std::string harmonic_type;
    };

    struct GpxRhythm {
        int id{-1};
        int augmentation_dot_count{0};
        int primary_tuplet_num{1};
        int primary_tuplet_den{1};
        std::string note_value;
    };

    struct GpxChord {
        int id{-1};
        std::string name;
        int string_count{0};
        int base_fret{0};
        std::vector<std::optional<int>> frets;
    };

    struct GpxDocument {
        SongMetadata score;
        std::vector<GpxTrackData> tracks;
        std::vector<GpxMasterBar> master_bars;
        std::vector<GpxBar> bars;
        std::vector<GpxVoice> voices;
        std::vector<GpxBeat> beats;
        std::vector<GpxNote> notes;
        std::vector<GpxChord> chords;
        std::vector<GpxRhythm> rhythms;
        std::vector<GpxAutomation> automations;

        template <typename T>
        [[nodiscard]] const T *find_by_id(const std::vector<T> &items, int id) const {
            for (const T &item : items) {
                if (item.id == id) {
                    return &item;
                }
            }
            return nullptr;
        }

        [[nodiscard]] const GpxBar *bar(int id) const { return find_by_id(bars, id); }
        [[nodiscard]] const GpxVoice *voice(int id) const { return find_by_id(voices, id); }
        [[nodiscard]] const GpxBeat *beat(int id) const { return find_by_id(beats, id); }
        [[nodiscard]] const GpxNote *note(int id) const { return find_by_id(notes, id); }
        [[nodiscard]] const GpxChord *chord(int id) const { return find_by_id(chords, id); }
        [[nodiscard]] const GpxRhythm *rhythm(int id) const { return find_by_id(rhythms, id); }

        [[nodiscard]] const GpxAutomation *automation(const std::string &type,
                                                      int until_bar_id) const {
            const GpxAutomation *result = nullptr;
            for (const GpxAutomation &item : automations) {
                if (item.type != type) {
                    continue;
                }
                if (item.bar_id <= until_bar_id &&
                    (result == nullptr || item.bar_id > result->bar_id)) {
                    result = &item;
                }
            }
            return result;
        }
    };

    /// Parses score.gpif into the GPX intermediate model (GPXDocumentReader.read).
    [[nodiscard]] ParseResult<GpxDocument> parse_gpx_document(std::string_view xml, bool is_gp7);

    /// Maps GpxDocument to Song (GPXDocumentParser.parse).
    [[nodiscard]] Song map_gpx_document(const GpxDocument &document);

} // namespace libgp_parser
