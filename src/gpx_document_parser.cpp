#include "libgp_parser/gpx_document.hpp"
#include "libgp_parser/tempo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace libgp_parser {
    namespace {

        constexpr float kGpBendPosition = 100.0F;
        constexpr float kGpBendSemitone = 25.0F;
        constexpr float kGpWhammyBarPosition = 100.0F;
        constexpr float kGpWhammyBarSemitone = 50.0F;

        struct Drumkit {
            int midi_value;
            int element;
            int variation;
        };

        constexpr Drumkit kDrumkits[] = {
            {36, 0, 0}, {36, 0, 0}, {37, 1, 2}, {38, 1, 0}, {41, 5, 0},  {42, 10, 0},
            {43, 6, 0}, {44, 11, 0}, {45, 7, 0}, {46, 10, 2}, {47, 8, 0}, {48, 9, 0},
            {49, 12, 0}, {50, 9, 0}, {51, 15, 0}, {52, 16, 0}, {53, 15, 2}, {55, 14, 0},
            {56, 3, 0}, {57, 13, 0}, {59, 15, 1},
        };

        int parse_triplet_feel(const GpxMasterBar &bar) {
            if (bar.triplet_feel == "Triplet8th") {
                return static_cast<int>(TripletFeel::Eighth);
            }
            if (bar.triplet_feel == "Triplet16th") {
                return static_cast<int>(TripletFeel::Sixteenth);
            }
            return static_cast<int>(TripletFeel::None);
        }

        int parse_dynamic(const GpxBeat &beat) {
            if (beat.dynamic == "PPP") {
                return Velocity::PianoPianissimo;
            }
            if (beat.dynamic == "PP") {
                return Velocity::Pianissimo;
            }
            if (beat.dynamic == "P") {
                return Velocity::Piano;
            }
            if (beat.dynamic == "MP") {
                return Velocity::MezzoPiano;
            }
            if (beat.dynamic == "MF") {
                return Velocity::MezzoForte;
            }
            if (beat.dynamic == "F") {
                return Velocity::Forte;
            }
            if (beat.dynamic == "FF") {
                return Velocity::Fortissimo;
            }
            if (beat.dynamic == "FFF") {
                return Velocity::ForteFortissimo;
            }
            return Velocity::Default;
        }

        StrokeDirection parse_stroke(const GpxBeat &beat) {
            if (beat.brush == "Down") {
                return StrokeDirection::Down;
            }
            if (beat.brush == "Up") {
                return StrokeDirection::Up;
            }
            return StrokeDirection::None;
        }

        StrokeDirection parse_pick_stroke(const GpxBeat &beat) {
            if (beat.pick_stroke == "Down") {
                return StrokeDirection::Down;
            }
            if (beat.pick_stroke == "Up") {
                return StrokeDirection::Up;
            }
            return StrokeDirection::None;
        }

        void parse_rhythm(const GpxRhythm &rhythm, Duration &duration) {
            duration.dotted = rhythm.augmentation_dot_count == 1;
            duration.double_dotted = rhythm.augmentation_dot_count == 2;
            duration.division.times = rhythm.primary_tuplet_den;
            duration.division.enters = rhythm.primary_tuplet_num;
            if (rhythm.note_value == "Whole") {
                duration.value = DurationValue::Whole;
            } else if (rhythm.note_value == "Half") {
                duration.value = DurationValue::Half;
            } else if (rhythm.note_value == "Quarter") {
                duration.value = DurationValue::Quarter;
            } else if (rhythm.note_value == "Eighth") {
                duration.value = DurationValue::Eighth;
            } else if (rhythm.note_value == "16th") {
                duration.value = DurationValue::Sixteenth;
            } else if (rhythm.note_value == "32nd") {
                duration.value = DurationValue::ThirtySecond;
            } else if (rhythm.note_value == "64th") {
                duration.value = DurationValue::SixtyFourth;
            }
        }

        int parse_bend_value(const int gp_value) {
            return static_cast<int>(
                std::lround(gp_value * (EffectBend::SemitoneLength / kGpBendSemitone)));
        }

        int parse_bend_position(const int gp_offset) {
            return static_cast<int>(
                std::lround(gp_offset * (EffectBend::MaxPositionLength / kGpBendPosition)));
        }

        std::optional<EffectBend> parse_bend(const GpxNote &note) {
            if (!note.bend_enabled || !note.bend_origin_value || !note.bend_destination_value) {
                return std::nullopt;
            }
            EffectBend bend;
            bend.points.push_back({.position = 0, .value = parse_bend_value(*note.bend_origin_value)});
            if (note.bend_origin_offset) {
                bend.points.push_back({.position = parse_bend_position(*note.bend_origin_offset),
                                       .value = parse_bend_value(*note.bend_origin_value)});
            }
            if (note.bend_middle_value) {
                const int default_middle = static_cast<int>(std::lround(kGpBendPosition / 2));
                if (!note.bend_middle_offset1 || *note.bend_middle_offset1 != 12) {
                    const int offset = note.bend_middle_offset1.value_or(default_middle);
                    bend.points.push_back({.position = parse_bend_position(offset),
                                           .value = parse_bend_value(*note.bend_middle_value)});
                }
                if (!note.bend_middle_offset2 || *note.bend_middle_offset2 != 12) {
                    const int offset = note.bend_middle_offset2.value_or(default_middle);
                    bend.points.push_back({.position = parse_bend_position(offset),
                                           .value = parse_bend_value(*note.bend_middle_value)});
                }
            }
            if (note.bend_destination_offset && *note.bend_destination_offset < kGpBendPosition) {
                bend.points.push_back(
                    {.position = parse_bend_position(*note.bend_destination_offset),
                     .value = parse_bend_value(*note.bend_destination_value)});
            }
            bend.points.push_back({.position = EffectBend::MaxPositionLength,
                                   .value = parse_bend_value(*note.bend_destination_value)});
            return bend;
        }

        int parse_tremolo_bar_value(const int gp_value) {
            int value = static_cast<int>(std::lround(gp_value * (1.0F / kGpWhammyBarSemitone)));
            if (value > EffectTremoloBar::MaxValueLength) {
                value = EffectTremoloBar::MaxValueLength;
            }
            if (value < -EffectTremoloBar::MaxValueLength) {
                value = -EffectTremoloBar::MaxValueLength;
            }
            return value;
        }

        int parse_tremolo_bar_position(const int gp_offset) {
            return static_cast<int>(std::lround(
                gp_offset * (EffectTremoloBar::MaxPositionLength / kGpWhammyBarPosition)));
        }

        std::optional<EffectTremoloBar> parse_tremolo_bar(const GpxBeat &beat) {
            if (!beat.whammy_bar_enabled || !beat.whammy_bar_origin_value ||
                !beat.whammy_bar_destination_value) {
                return std::nullopt;
            }
            EffectTremoloBar bar;
            bar.points.push_back(
                {.position = 0, .value = parse_tremolo_bar_value(*beat.whammy_bar_origin_value)});
            if (beat.whammy_bar_origin_offset) {
                bar.points.push_back(
                    {.position = parse_tremolo_bar_position(*beat.whammy_bar_origin_offset),
                     .value = parse_tremolo_bar_value(*beat.whammy_bar_origin_value)});
            }
            if (beat.whammy_bar_middle_value) {
                bool hidden = false;
                if (*beat.whammy_bar_destination_value != 0) {
                    hidden = false;
                }
                if (!hidden) {
                    const int default_middle =
                        static_cast<int>(std::lround(kGpWhammyBarPosition / 2));
                    const int offset1 = beat.whammy_bar_middle_offset1.value_or(default_middle);
                    if (!beat.whammy_bar_origin_offset ||
                        offset1 >= *beat.whammy_bar_origin_offset) {
                        bar.points.push_back(
                            {.position = parse_tremolo_bar_position(offset1),
                             .value = parse_tremolo_bar_value(*beat.whammy_bar_middle_value)});
                    }
                    const int offset2 = beat.whammy_bar_middle_offset2.value_or(default_middle);
                    if (!beat.whammy_bar_origin_offset ||
                        (offset1 >= *beat.whammy_bar_origin_offset && offset2 > offset1)) {
                        bar.points.push_back(
                            {.position = parse_tremolo_bar_position(offset2),
                             .value = parse_tremolo_bar_value(*beat.whammy_bar_middle_value)});
                    }
                }
            }
            if (beat.whammy_bar_destination_offset &&
                *beat.whammy_bar_destination_offset < kGpWhammyBarPosition) {
                bar.points.push_back(
                    {.position = parse_tremolo_bar_position(*beat.whammy_bar_destination_offset),
                     .value = parse_tremolo_bar_value(*beat.whammy_bar_destination_value)});
            }
            bar.points.push_back(
                {.position = EffectTremoloBar::MaxPositionLength,
                 .value = parse_tremolo_bar_value(*beat.whammy_bar_destination_value)});
            return bar;
        }

        std::optional<EffectHarmonic> parse_harmonic(const GpxNote &note) {
            if (note.harmonic_type.empty()) {
                return std::nullopt;
            }
            EffectHarmonic harmonic;
            if (note.harmonic_type == "Artificial") {
                harmonic.type = HarmonicType::Artificial;
            } else if (note.harmonic_type == "Natural") {
                harmonic.type = HarmonicType::Natural;
            } else if (note.harmonic_type == "Pinch") {
                harmonic.type = HarmonicType::Pinch;
            } else {
                harmonic.type = HarmonicType::Natural;
            }
            if (note.harmonic_fret >= 0) {
                for (int i = 0; i < 6; ++i) {
                    if (note.harmonic_fret == kNaturalFrequencies[i][0]) {
                        harmonic.data = i;
                        break;
                    }
                }
            }
            return harmonic;
        }

        std::optional<EffectTrill> parse_trill(const GpxNote &note, const int initial_fret) {
            if (note.trill <= 0) {
                return std::nullopt;
            }
            EffectTrill trill;
            const int diff = note.trill - note.midi_number;
            trill.fret = initial_fret + diff;
            trill.duration = duration_from_time(static_cast<long>(note.trill_duration) * 2);
            return trill;
        }

        std::optional<EffectTremoloPicking> parse_tremolo_picking(const GpxBeat &beat) {
            if (beat.tremolo.size() != 2) {
                return std::nullopt;
            }
            EffectTremoloPicking picking;
            picking.duration.value = DurationValue::Quarter * beat.tremolo[1];
            return picking;
        }

        std::optional<EffectGrace> parse_grace(const GpxBeat *previous_beat,
                                               const std::vector<const GpxNote *> &grace_notes,
                                               const Duration *previous_duration,
                                               const GpxNote &current) {
            if (previous_beat == nullptr || previous_duration == nullptr ||
                previous_beat->grace_notes.empty()) {
                return std::nullopt;
            }
            const GpxNote *grace_note = nullptr;
            for (const GpxNote *candidate : grace_notes) {
                if (candidate->string == current.string) {
                    grace_note = candidate;
                    break;
                }
            }
            if (grace_note == nullptr) {
                return std::nullopt;
            }
            EffectGrace grace;
            grace.on_beat = previous_beat->grace_notes == "OnBeat";
            switch (previous_duration->value) {
            case DurationValue::Sixteenth:
                grace.duration = GraceDuration::Sixteenth;
                break;
            case DurationValue::ThirtySecond:
                grace.duration = GraceDuration::ThirtySecond;
                break;
            default:
                grace.duration = GraceDuration::SixtyFourth;
                break;
            }
            if (grace_note->bend_enabled) {
                grace.transition = GraceTransition::Bend;
            } else if (grace_note->slide) {
                grace.transition = GraceTransition::Slide;
            } else if (grace_note->hammer) {
                grace.transition = GraceTransition::Hammer;
            }
            grace.dead = grace_note->muted_enabled;
            grace.dynamic = parse_dynamic(*previous_beat);
            grace.fret = grace_note->fret;
            return grace;
        }

        const int *string_for(const Track &track, const Beat &beat, const int midi_value) {
            for (int i = 0; i < track.string_count(); ++i) {
                const int pitch = track.tuning_pitches[static_cast<std::size_t>(i)];
                if (midi_value < pitch) {
                    continue;
                }
                const int string_number = i + 1;
                bool empty = true;
                for (const Voice &voice : beat.voices) {
                    for (const Note &note : voice.notes) {
                        if (note.string == string_number) {
                            empty = false;
                            break;
                        }
                    }
                }
                if (empty) {
                    return &track.tuning_pitches[static_cast<std::size_t>(i)];
                }
            }
            return nullptr;
        }

        void parse_note(const GpxDocument &document, const GpxNote &gp_note, Voice &voice,
                        const int velocity, const GpxBeat &gp_beat, const GpxBeat *previous_beat,
                        const std::vector<const GpxNote *> &grace_notes,
                        const Duration *previous_duration, const Track &track, const Beat &beat) {
            int tg_value = -1;
            int tg_string = -1;
            if (gp_note.string >= 0 && gp_note.fret >= 0) {
                tg_value = gp_note.fret;
                tg_string = track.string_count() - gp_note.string;
            } else {
                int gm_value = -1;
                if (gp_note.midi_number >= 0) {
                    gm_value = gp_note.midi_number;
                } else if (gp_note.tone >= 0 && gp_note.octave >= 0) {
                    gm_value = gp_note.tone + ((12 * gp_note.octave) - 12);
                } else if (gp_note.element >= 0) {
                    for (const Drumkit &kit : kDrumkits) {
                        if (kit.element == gp_note.element && kit.variation == gp_note.variation) {
                            gm_value = kit.midi_value;
                            break;
                        }
                    }
                }
                if (gm_value >= 0) {
                    const int *pitch = string_for(track, beat, gm_value);
                    if (pitch != nullptr) {
                        tg_value = gm_value - *pitch;
                        tg_string = static_cast<int>(pitch - track.tuning_pitches.data()) + 1;
                    }
                }
            }

            if (tg_value < 0 || tg_string <= 0) {
                return;
            }

            Note note;
            note.value = tg_value;
            note.string = tg_string;
            note.tied_note = gp_note.tie_destination;
            note.velocity = velocity;
            note.effect.fade_in = gp_beat.fadding == "FadeIn";
            note.effect.vibrato = gp_note.vibrato;
            note.effect.let_ring = gp_note.let_ring;
            note.effect.slide = gp_note.slide;
            note.effect.dead_note = gp_note.muted_enabled;
            note.effect.palm_mute = gp_note.palm_muted_enabled;
            note.effect.tapping = gp_note.tapped;
            note.effect.hammer = gp_note.hammer;
            note.effect.ghost_note = gp_note.ghost;
            note.effect.slapping = gp_beat.slapped;
            note.effect.popping = gp_beat.popped;
            note.effect.staccato = gp_note.accent == 1;
            note.effect.heavy_accentuated_note = gp_note.accent == 4;
            note.effect.accentuated_note = gp_note.accent == 8;
            note.effect.trill = parse_trill(gp_note, tg_value);
            note.effect.tremolo_picking = parse_tremolo_picking(gp_beat);
            note.effect.harmonic = parse_harmonic(gp_note);
            note.effect.bend = parse_bend(gp_note);
            note.effect.tremolo_bar = parse_tremolo_bar(gp_beat);
            note.effect.grace = parse_grace(previous_beat, grace_notes, previous_duration, gp_note);
            voice.add_note(std::move(note));
        }

        void parse_bar(const GpxDocument &document, const GpxBar &bar, Measure &measure,
                       const Track &track) {
            if (bar.clef == "F4") {
                measure.clef = Clef::Bass;
            } else if (bar.clef == "C3") {
                measure.clef = Clef::Alto;
            } else if (bar.clef == "C4") {
                measure.clef = Clef::Tenor;
            }

            for (int v = 0; v < kMaxVoices; ++v) {
                if (static_cast<int>(bar.voice_ids.size()) <= v || bar.voice_ids[static_cast<std::size_t>(v)] < 0) {
                    continue;
                }
                const GpxVoice *voice = document.voice(bar.voice_ids[static_cast<std::size_t>(v)]);
                if (voice == nullptr) {
                    continue;
                }

                long tg_start = 0;
                const GpxBeat *previous_beat = nullptr;
                std::vector<const GpxNote *> previous_grace;
                Duration previous_duration;
                bool has_previous_duration = false;

                for (const int beat_id : voice->beat_ids) {
                    const GpxBeat *beat = document.beat(beat_id);
                    if (beat == nullptr) {
                        continue;
                    }
                    const GpxRhythm *rhythm = document.rhythm(beat->rhythm_id);
                    if (rhythm == nullptr) {
                        continue;
                    }

                    if (!beat->grace_notes.empty()) {
                        previous_grace.clear();
                        for (const int note_id : beat->note_ids) {
                            if (const GpxNote *gp_note = document.note(note_id)) {
                                previous_grace.push_back(gp_note);
                            }
                        }
                        previous_beat = beat;
                        parse_rhythm(*rhythm, previous_duration);
                        has_previous_duration = true;
                        continue;
                    }

                    Beat &tg_beat = get_or_create_beat(measure, tg_start);
                    Voice &tg_voice = tg_beat.voice(v);
                    tg_voice.empty = false;
                    tg_beat.stroke.direction = parse_stroke(*beat);
                    tg_beat.pick_stroke.direction = parse_pick_stroke(*beat);
                    if (!beat->text.empty()) {
                        tg_beat.text = beat->text;
                        while (!tg_beat.text.empty() &&
                               (tg_beat.text.back() == ' ' || tg_beat.text.back() == '\t')) {
                            tg_beat.text.pop_back();
                        }
                    }
                    if (beat->chord_id) {
                        if (const GpxChord *gp_chord = document.chord(*beat->chord_id)) {
                            Chord chord;
                            chord.name = gp_chord->name;
                            chord.first_fret = gp_chord->base_fret;
                            chord.frets.assign(static_cast<std::size_t>(std::max(gp_chord->string_count, 0)), -1);
                            for (int s = 0; s < gp_chord->string_count; ++s) {
                                if (static_cast<std::size_t>(s) < gp_chord->frets.size() &&
                                    gp_chord->frets[static_cast<std::size_t>(s)]) {
                                    const int dest = gp_chord->string_count - s - 1;
                                    if (dest >= 0 && dest < gp_chord->string_count) {
                                        chord.frets[static_cast<std::size_t>(dest)] =
                                            *gp_chord->frets[static_cast<std::size_t>(s)] +
                                            gp_chord->base_fret;
                                    }
                                }
                            }
                            tg_beat.chord = std::move(chord);
                        }
                    }
                    parse_rhythm(*rhythm, tg_voice.duration);
                    const int velocity = parse_dynamic(*beat);
                    for (const int note_id : beat->note_ids) {
                        if (const GpxNote *gp_note = document.note(note_id)) {
                            parse_note(document, *gp_note, tg_voice, velocity, *beat, previous_beat,
                                       previous_grace,
                                       has_previous_duration ? &previous_duration : nullptr, track,
                                       tg_beat);
                        }
                    }
                    tg_start += tg_voice.duration.time();
                    previous_beat = beat;
                    previous_grace.clear();
                    previous_duration = tg_voice.duration;
                    has_previous_duration = true;
                }
            }
        }

        void apply_beat_starts(Measure &measure, const long header_start) {
            for (Beat &beat : measure.beats) {
                beat.start += header_start;
            }
        }

        void fix_first_measure_starts(Measure &measure, const MeasureHeader &header) {
            if (header.number != 1) {
                return;
            }
            const long measure_end = header.start + header.length();
            long maximum_end = 0;
            for (const Beat &beat : measure.beats) {
                for (const Voice &voice : beat.voices) {
                    if (!voice.empty) {
                        maximum_end = std::max(maximum_end, beat.start + voice.duration.time());
                    }
                }
            }
            if (maximum_end < measure_end) {
                const long movement = measure_end - maximum_end;
                for (Beat &beat : measure.beats) {
                    beat.start += movement;
                }
            }
        }

        void parse_tracks(Song &song, const GpxDocument &document) {
            for (std::size_t i = 0; i < document.tracks.size(); ++i) {
                const GpxTrackData &gp_track = document.tracks[i];
                Channel channel;
                channel.bank = gp_track.gm_channel1 == kDefaultPercussionChannel
                                   ? kDefaultPercussionBank
                                   : kDefaultBank;
                channel.program =
                    channel.is_percussion() ? 0 : gp_track.gm_program;

                const std::string gm1 = std::to_string(gp_track.gm_channel1);
                const std::string gm2 = std::to_string(
                    gp_track.gm_channel1 != kDefaultPercussionChannel ? gp_track.gm_channel2
                                                                      : gp_track.gm_channel1);

                for (const Channel &existing : song.channels) {
                    if (const ChannelParameter *param = find_parameter(existing, kGmChannel1Key)) {
                        if (param->value == gm1) {
                            channel.channel_id = existing.channel_id;
                        }
                    }
                }
                if (channel.channel_id <= 0) {
                    channel.channel_id = static_cast<int>(song.channels.size()) + 1;
                    channel.name = "untitled";
                    set_parameter(channel, kGmChannel1Key, gm1);
                    set_parameter(channel, kGmChannel2Key, gm2);
                    song.channels.push_back(channel);
                }

                Track track;
                track.id = gp_track.id;
                track.number = static_cast<int>(i) + 1;
                track.name = gp_track.name;
                track.channel_id = channel.channel_id;
                track.gm_program = gp_track.gm_program;
                track.gm_channel1 = gp_track.gm_channel1;
                track.gm_channel2 = gp_track.gm_channel2;
                track.capo = gp_track.capo;
                if (gp_track.color.size() >= 3) {
                    track.color = RgbColor{
                        .r = static_cast<std::uint8_t>(std::clamp(gp_track.color[0], 0, 255)),
                        .g = static_cast<std::uint8_t>(std::clamp(gp_track.color[1], 0, 255)),
                        .b = static_cast<std::uint8_t>(std::clamp(gp_track.color[2], 0, 255)),
                    };
                }
                if (!gp_track.tuning_pitches.empty()) {
                    for (int s = 1; s <= static_cast<int>(gp_track.tuning_pitches.size()); ++s) {
                        track.tuning_pitches.push_back(
                            gp_track.tuning_pitches[gp_track.tuning_pitches.size() -
                                                    static_cast<std::size_t>(s)]);
                    }
                } else if (channel.is_percussion()) {
                    track.tuning_pitches.assign(6, 0);
                } else {
                    track.tuning_pitches = {64, 59, 55, 50, 45, 40};
                }
                song.tracks.push_back(std::move(track));
            }
        }

        void parse_master_bars(Song &song, const GpxDocument &document) {
            long tg_start = kQuarterTime;
            for (std::size_t i = 0; i < document.master_bars.size(); ++i) {
                const GpxMasterBar &mbar = document.master_bars[i];
                const GpxAutomation *tempo_auto = document.automation("Tempo", static_cast<int>(i));

                MeasureHeader header;
                header.start = tg_start;
                header.number = static_cast<int>(i) + 1;
                header.repeat_open = mbar.repeat_start;
                header.repeat_close = mbar.repeat_count;
                if (!mbar.alternate_endings.empty()) {
                    int alternative = 0;
                    for (int ending : mbar.alternate_endings) {
                        --ending;
                        alternative |= 1 << ending;
                    }
                    header.repeat_alternative = alternative;
                }
                header.triplet_feel = static_cast<TripletFeel>(parse_triplet_feel(mbar));
                if (mbar.time.size() == 2) {
                    header.time_signature.numerator = mbar.time[0];
                    header.time_signature.denominator.value = mbar.time[1];
                }
                if (tempo_auto != nullptr && tempo_auto->value.size() >= 1) {
                    int bpm = tempo_auto->value[0];
                    if (tempo_auto->value.size() >= 2) {
                        bpm = apply_gpx_tempo_modifier(
                            {.bpm = bpm,
                             .note_value = static_cast<NoteType>(tempo_auto->value[1])});
                    }
                    header.tempo.quarter_bpm = bpm;
                }
                if (!mbar.marker_text.empty()) {
                    header.marker = Marker{.measure = header.number, .title = mbar.marker_text};
                }
                song.measure_headers.push_back(header);

                for (std::size_t t = 0; t < song.tracks.size(); ++t) {
                    Track &track = song.tracks[t];
                    Measure measure;
                    measure.header_index = static_cast<int>(i);
                    int accidental = mbar.accidental_count;
                    if (accidental < 0) {
                        accidental = 7 - accidental;
                    }
                    if (accidental >= 0 && accidental <= 14) {
                        measure.key_signature = accidental;
                    }

                    int gp_master_index = static_cast<int>(i);
                    const GpxBar *gp_bar =
                        t < mbar.bar_ids.size() ? document.bar(mbar.bar_ids[t]) : nullptr;
                    while (gp_bar != nullptr && !gp_bar->simile_mark.empty()) {
                        if (gp_bar->simile_mark == "Simple") {
                            --gp_master_index;
                        } else if (gp_bar->simile_mark == "FirstOfDouble" ||
                                   gp_bar->simile_mark == "SecondOfDouble") {
                            gp_master_index -= 2;
                        }
                        if (gp_master_index >= 0) {
                            const GpxMasterBar &copy =
                                document.master_bars[static_cast<std::size_t>(gp_master_index)];
                            gp_bar = t < copy.bar_ids.size() ? document.bar(copy.bar_ids[t]) : nullptr;
                        } else {
                            gp_bar = nullptr;
                        }
                    }
                    if (gp_bar != nullptr) {
                        parse_bar(document, *gp_bar, measure, track);
                        apply_beat_starts(measure, header.start);
                    }
                    if (header.number == 1) {
                        fix_first_measure_starts(measure, header);
                    }
                    track.measures.push_back(std::move(measure));
                }

                tg_start += header.length();
            }
        }

    } // namespace

    Song map_gpx_document(const GpxDocument &document) {
        Song song;
        song.metadata = document.score;
        parse_tracks(song, document);
        parse_master_bars(song, document);
        if (!song.measure_headers.empty()) {
            song.tempo_bpm = song.measure_headers.front().tempo.quarter_bpm;
        } else if (const GpxAutomation *tempo = document.automation("Tempo", 0)) {
            if (!tempo->value.empty()) {
                int bpm = tempo->value[0];
                if (tempo->value.size() >= 2) {
                    bpm = apply_gpx_tempo_modifier(
                        {.bpm = bpm, .note_value = static_cast<NoteType>(tempo->value[1])});
                }
                song.tempo_bpm = bpm;
            }
        }
        return song;
    }

} // namespace libgp_parser
