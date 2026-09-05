#include "libgp_parser/binary_reader.hpp"
#include "libgp_parser/channel.hpp"
#include "libgp_parser/duration.hpp"
#include "libgp_parser/error.hpp"
#include "libgp_parser/song.hpp"
#include "libgp_parser/types.hpp"
#include "libgp_parser/version_constants.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace libgp_parser {
    namespace {

        template <typename To, typename From>
        ParseResult<To> fail_from(const ParseResult<From> &result) {
            return ParseResult<To>::failure(result.error());
        }

        // TRACK_CHANNELS[i] = {channelId, gmChannel1, gmChannel2}
        constexpr int kTrackChannels[8][3] = {
            {1, 0, 1}, {2, 2, 3}, {3, 4, 5}, {4, 6, 7},
            {5, 8, 10}, {6, 11, 12}, {7, 13, 14}, {8, 9, 9},
        };

        ParseResult<Duration> read_gp1_duration(BinaryReader &stream) {
            auto signed_value = stream.read_i8();
            if (!signed_value) {
                return fail_from<Duration>(signed_value);
            }
            Duration duration;
            duration.value = 1 << (static_cast<int>(signed_value.value()) + 2);
            return ParseResult<Duration>::success(duration);
        }

        ParseResult<Ok> read_gp1_bend(BinaryReader &stream, NoteEffect &effect) {
            if (auto step = stream.skip(6); !step) {
                return fail_from<Ok>(step);
            }
            auto raw = stream.read_u8();
            if (!raw) {
                return fail_from<Ok>(raw);
            }
            const float value = std::max((static_cast<float>(raw.value()) / 8.0F) - 26.0F, 1.0F);
            EffectBend bend;
            bend.points.push_back({.position = 0, .value = 0});
            bend.points.push_back(
                {.position = EffectBend::MaxPositionLength / 2,
                 .value = static_cast<int>(std::lround(value * EffectBend::SemitoneLength))});
            bend.points.push_back(
                {.position = EffectBend::MaxPositionLength,
                 .value = static_cast<int>(std::lround(value * EffectBend::SemitoneLength))});
            effect.bend = std::move(bend);
            if (auto step = stream.skip(1); !step) {
                return fail_from<Ok>(step);
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_gp1_beat_effects(BinaryReader &stream, NoteEffect &effect) {
            auto flags = stream.read_u8();
            if (!flags) {
                return fail_from<Ok>(flags);
            }
            effect.vibrato = (flags.value() == 1 || flags.value() == 2);
            effect.fade_in = (flags.value() == 4);
            effect.tapping = (flags.value() == 5);
            effect.slapping = (flags.value() == 6);
            effect.popping = (flags.value() == 7);
            if (flags.value() == 3) {
                return read_gp1_bend(stream, effect);
            }
            if (flags.value() == 8 || flags.value() == 9) {
                EffectHarmonic harmonic;
                harmonic.type =
                    flags.value() == 8 ? HarmonicType::Natural : HarmonicType::Artificial;
                harmonic.data = 0;
                effect.harmonic = harmonic;
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_gp1_note_effects(BinaryReader &stream, NoteEffect &effect) {
            auto flags = stream.read_u8();
            if (!flags) {
                return fail_from<Ok>(flags);
            }
            effect.hammer = (flags.value() == 1 || flags.value() == 2);
            effect.slide = (flags.value() == 3 || flags.value() == 4);
            if (flags.value() == 5 || flags.value() == 6) {
                return read_gp1_bend(stream, effect);
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_gp1_text(BinaryReader &stream, Beat &beat) {
            auto text = stream.read_string_byte(0);
            if (!text) {
                return fail_from<Ok>(text);
            }
            beat.text = std::move(text.value());
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_gp1_chord(BinaryReader &stream, Beat &beat, const int strings,
                                      const int version_code) {
            if (version_code > 3) {
                Chord chord;
                chord.frets.assign(static_cast<std::size_t>(strings), -1);
                auto name = stream.read_string_byte(0);
                if (!name) {
                    return fail_from<Ok>(name);
                }
                chord.name = std::move(name.value());
                if (auto step = stream.skip(1); !step) {
                    return fail_from<Ok>(step);
                }
                auto header = stream.read_i32();
                if (!header) {
                    return fail_from<Ok>(header);
                }
                if (header.value() < 12) {
                    if (auto step = stream.skip(32); !step) {
                        return fail_from<Ok>(step);
                    }
                }
                auto first = stream.read_i32();
                if (!first) {
                    return fail_from<Ok>(first);
                }
                chord.first_fret = first.value();
                if (chord.first_fret != 0) {
                    for (int i = 0; i < 6; ++i) {
                        auto fret = stream.read_i32();
                        if (!fret) {
                            return fail_from<Ok>(fret);
                        }
                        if (i < strings) {
                            chord.frets[static_cast<std::size_t>(i)] = fret.value();
                        }
                    }
                }
                if (chord.note_count() > 0) {
                    beat.chord = std::move(chord);
                }
            } else {
                if (auto step = stream.read_string_byte_size_of_byte(); !step) {
                    return fail_from<Ok>(step);
                }
            }
            return ParseResult<Ok>::success(Ok{});
        }

        Beat *find_beat_in_measure(Measure &measure, const long start) {
            return find_beat_at(measure, start);
        }

        Beat *find_beat_on_track(Track &track, Measure &measure, const long start) {
            if (Beat *beat = find_beat_in_measure(measure, start)) {
                return beat;
            }
            for (int i = static_cast<int>(track.measures.size()) - 1; i >= 0; --i) {
                if (Beat *beat = find_beat_in_measure(track.measures[static_cast<std::size_t>(i)],
                                                     start)) {
                    return beat;
                }
            }
            return nullptr;
        }

        ParseResult<long> read_gp1_beat(BinaryReader &stream, Track &track, Measure &measure,
                                        const long start, const long last_start,
                                        const int version_code) {
            if (auto pad = stream.read_i32(); !pad) {
                return fail_from<long>(pad);
            }
            Beat beat;
            beat.start = start;
            Voice &voice = beat.voice(0);
            auto duration = read_gp1_duration(stream);
            if (!duration) {
                return fail_from<long>(duration);
            }
            NoteEffect effect;
            auto flags = stream.read_u8();
            if (!flags) {
                return fail_from<long>(flags);
            }
            duration.value().dotted = (flags.value() & 0x10) != 0;
            if ((flags.value() & 0x20) != 0) {
                duration.value().division.enters = 3;
                duration.value().division.times = 2;
                if (auto step = stream.skip(1); !step) {
                    return fail_from<long>(step);
                }
            }
            if ((flags.value() & 0x04) != 0) {
                if (auto step = read_gp1_beat_effects(stream, effect); !step) {
                    return fail_from<long>(step);
                }
            }
            if ((flags.value() & 0x02) != 0) {
                if (auto step = read_gp1_chord(stream, beat, track.string_count(), version_code);
                    !step) {
                    return fail_from<long>(step);
                }
            }
            if ((flags.value() & 0x01) != 0) {
                if (auto step = read_gp1_text(stream, beat); !step) {
                    return fail_from<long>(step);
                }
            }
            if ((flags.value() & 0x40) != 0) {
                if (last_start < start) {
                    if (Beat *previous = find_beat_on_track(track, measure, last_start)) {
                        for (const Note &prev : previous->voice(0).notes) {
                            Note note;
                            note.value = prev.value;
                            note.string = prev.string;
                            note.velocity = prev.velocity;
                            note.tied_note = true;
                            voice.add_note(std::move(note));
                        }
                    }
                }
            } else if ((flags.value() & 0x08) == 0) {
                auto string_flags = stream.read_u8();
                if (!string_flags) {
                    return fail_from<long>(string_flags);
                }
                auto effects_flags = stream.read_u8();
                if (!effects_flags) {
                    return fail_from<long>(effects_flags);
                }
                for (int i = 5; i >= 0; --i) {
                    if ((string_flags.value() & (1 << i)) == 0) {
                        continue;
                    }
                    Note note;
                    auto fret = stream.read_u8();
                    if (!fret) {
                        return fail_from<long>(fret);
                    }
                    if ((effects_flags.value() & (1 << i)) != 0) {
                        if (auto step = read_gp1_note_effects(stream, effect); !step) {
                            return fail_from<long>(step);
                        }
                    }
                    const int fret_value = static_cast<int>(fret.value());
                    note.value = (fret_value >= 0 && fret_value < 100) ? fret_value : 0;
                    note.velocity = Velocity::Default;
                    note.string = track.string_count() - i;
                    note.effect = effect;
                    note.effect.dead_note = (fret_value < 0 || fret_value >= 100);
                    voice.add_note(std::move(note));
                }
            }
            voice.empty = false;
            voice.duration = duration.value();
            measure.beats.push_back(std::move(beat));
            return ParseResult<long>::success(duration.value().time());
        }

        int parse_repeat_alternative(const Song &song, const int measure_number, const int value) {
            int repeat_alternative = 0;
            int existent = 0;
            for (const MeasureHeader &header : song.measure_headers) {
                if (header.number == measure_number) {
                    break;
                }
                if (header.repeat_open) {
                    existent = 0;
                }
                existent |= header.repeat_alternative;
            }
            for (int i = 0; i < 8; ++i) {
                if (value > i && (existent & (1 << i)) == 0) {
                    repeat_alternative |= (1 << i);
                }
            }
            return repeat_alternative;
        }

        Clef clef_for_track(const Song &song, const Track &track) {
            const Channel *channel = nullptr;
            for (const Channel &candidate : song.channels) {
                if (candidate.channel_id == track.channel_id) {
                    channel = &candidate;
                    break;
                }
            }
            if (channel != nullptr && channel->is_percussion()) {
                return Clef::Treble;
            }
            for (const int pitch : track.tuning_pitches) {
                if (pitch <= 34) {
                    return Clef::Bass;
                }
            }
            return Clef::Treble;
        }

        ParseResult<Ok> read_gp1_track(BinaryReader &stream, Track &track, Channel &channel,
                                       const int version_code) {
            track.name = "Track 1";
            auto program = stream.read_i32();
            if (!program) {
                return fail_from<Ok>(program);
            }
            channel.program = program.value();
            track.gm_program = program.value();
            if (version_code > 2) {
                if (auto frets = stream.read_i32(); !frets) {
                    return fail_from<Ok>(frets);
                }
                auto name = stream.read_string_byte_size_of_byte();
                if (!name) {
                    return fail_from<Ok>(name);
                }
                track.name = std::move(name.value());
                if (auto solo = stream.read_bool(); !solo) {
                    return fail_from<Ok>(solo);
                }
                auto volume = stream.read_i32();
                if (!volume) {
                    return fail_from<Ok>(volume);
                }
                channel.volume = volume.value();
                auto balance = stream.read_i32();
                if (!balance) {
                    return fail_from<Ok>(balance);
                }
                channel.balance = balance.value();
                auto chorus = stream.read_i32();
                if (!chorus) {
                    return fail_from<Ok>(chorus);
                }
                channel.chorus = chorus.value();
                auto reverb = stream.read_i32();
                if (!reverb) {
                    return fail_from<Ok>(reverb);
                }
                channel.reverb = reverb.value();
                auto offset = stream.read_i32();
                if (!offset) {
                    return fail_from<Ok>(offset);
                }
                track.capo = offset.value();
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_gp1_track_measures(BinaryReader &stream, Song &song,
                                                MeasureHeader &header,
                                                std::vector<long> &last_starts,
                                                const int track_count, const int version_code,
                                                const int key_signature) {
            auto numerator = stream.read_u8();
            if (!numerator) {
                return fail_from<Ok>(numerator);
            }
            auto denominator = stream.read_u8();
            if (!denominator) {
                return fail_from<Ok>(denominator);
            }
            header.time_signature.numerator = static_cast<int>(numerator.value());
            header.time_signature.denominator.value = static_cast<int>(denominator.value());
            if (auto step = stream.skip(6); !step) {
                return fail_from<Ok>(step);
            }

            std::vector<int> beats(static_cast<std::size_t>(track_count), 0);
            for (int i = 0; i < track_count; ++i) {
                if (auto a = stream.read_u8(); !a) {
                    return fail_from<Ok>(a);
                }
                if (auto b = stream.read_u8(); !b) {
                    return fail_from<Ok>(b);
                }
                auto count = stream.read_u8();
                if (!count) {
                    return fail_from<Ok>(count);
                }
                beats[static_cast<std::size_t>(i)] =
                    count.value() > 127 ? 0 : static_cast<int>(count.value());
                if (auto step = stream.skip(9); !step) {
                    return fail_from<Ok>(step);
                }
            }
            if (auto step = stream.skip(2); !step) {
                return fail_from<Ok>(step);
            }
            auto flags = stream.read_u8();
            if (!flags) {
                return fail_from<Ok>(flags);
            }
            header.repeat_open = (flags.value() & 0x01) != 0;
            if ((flags.value() & 0x02) != 0) {
                auto close = stream.read_u8();
                if (!close) {
                    return fail_from<Ok>(close);
                }
                header.repeat_close = static_cast<int>(close.value());
            }
            if ((flags.value() & 0x04) != 0) {
                auto alt = stream.read_u8();
                if (!alt) {
                    return fail_from<Ok>(alt);
                }
                header.repeat_alternative =
                    parse_repeat_alternative(song, header.number, static_cast<int>(alt.value()));
            }

            song.measure_headers.push_back(header);
            const int header_index = static_cast<int>(song.measure_headers.size()) - 1;
            for (int i = 0; i < track_count; ++i) {
                Track &track = song.tracks[static_cast<std::size_t>(i)];
                Measure measure;
                measure.header_index = header_index;
                measure.key_signature = key_signature;
                measure.clef = clef_for_track(song, track);
                long start = song.measure_headers.back().start;
                for (int j = 0; j < beats[static_cast<std::size_t>(i)]; ++j) {
                    auto advance = read_gp1_beat(stream, track, measure, start,
                                                 last_starts[static_cast<std::size_t>(i)],
                                                 version_code);
                    if (!advance) {
                        return fail_from<Ok>(advance);
                    }
                    last_starts[static_cast<std::size_t>(i)] = start;
                    start += advance.value();
                }
                track.measures.push_back(std::move(measure));
            }
            return ParseResult<Ok>::success(Ok{});
        }

    } // namespace

    ParseResult<Song> load_gp1_song(BinaryReader &stream, const int version_code) {
        const int track_count = version_code > 2 ? 8 : 1;
        Song song;

        auto title = stream.read_string_byte_size_of_byte();
        if (!title) {
            return fail_from<Song>(title);
        }
        song.metadata.title = std::move(title.value());
        auto author = stream.read_string_byte_size_of_byte();
        if (!author) {
            return fail_from<Song>(author);
        }
        song.metadata.words_and_music = std::move(author.value());
        if (auto unused = stream.read_string_byte_size_of_byte(); !unused) {
            return fail_from<Song>(unused);
        }

        auto tempo = stream.read_i32();
        if (!tempo) {
            return fail_from<Song>(tempo);
        }
        song.tempo_bpm = tempo.value();
        auto triplet = stream.read_i32();
        if (!triplet) {
            return fail_from<Song>(triplet);
        }
        const TripletFeel triplet_feel =
            triplet.value() == 1 ? TripletFeel::Eighth : TripletFeel::None;

        int key_signature = 0;
        if (version_code > 2) {
            auto key = stream.read_i32();
            if (!key) {
                return fail_from<Song>(key);
            }
            key_signature = key.value();
        }

        for (int i = 0; i < track_count; ++i) {
            Channel channel;
            channel.channel_id = kTrackChannels[i][0];
            set_parameter(channel, kGmChannel1Key, std::to_string(kTrackChannels[i][1]));
            set_parameter(channel, kGmChannel2Key, std::to_string(kTrackChannels[i][2]));
            if (kTrackChannels[i][1] == kDefaultPercussionChannel) {
                channel.bank = kDefaultPercussionBank;
            }
            song.channels.push_back(std::move(channel));
        }

        for (int i = 0; i < track_count; ++i) {
            Track track;
            track.number = i + 1;
            track.channel_id = kTrackChannels[i][0];
            track.gm_channel1 = kTrackChannels[i][1];
            track.gm_channel2 = kTrackChannels[i][2];
            track.color = RgbColor{.r = 255, .g = 0, .b = 0};
            if (version_code > 1) {
                auto count = stream.read_i32();
                if (!count) {
                    return fail_from<Song>(count);
                }
                for (int j = 0; j < count.value(); ++j) {
                    auto pitch = stream.read_i32();
                    if (!pitch) {
                        return fail_from<Song>(pitch);
                    }
                    track.tuning_pitches.push_back(pitch.value());
                }
            } else {
                for (int j = 0; j < 6; ++j) {
                    auto pitch = stream.read_i32();
                    if (!pitch) {
                        return fail_from<Song>(pitch);
                    }
                    track.tuning_pitches.push_back(pitch.value());
                }
            }
            song.tracks.push_back(std::move(track));
        }

        auto measure_count = stream.read_i32();
        if (!measure_count) {
            return fail_from<Song>(measure_count);
        }
        if (measure_count.value() < 0 || measure_count.value() > 10000) {
            return ParseResult<Song>::failure(
                {.code = ParseErrorCode::Unsupported, .message = "Invalid measure count"});
        }

        for (int i = 0; i < track_count; ++i) {
            if (auto step = read_gp1_track(stream, song.tracks[static_cast<std::size_t>(i)],
                                           song.channels[static_cast<std::size_t>(i)], version_code);
                !step) {
                return fail_from<Song>(step);
            }
        }

        if (version_code > 2) {
            if (auto step = stream.skip(10); !step) {
                return fail_from<Song>(step);
            }
        }

        std::vector<long> last_starts(static_cast<std::size_t>(track_count), 0);
        MeasureHeader *previous = nullptr;
        for (int i = 0; i < measure_count.value(); ++i) {
            MeasureHeader header;
            header.start = previous == nullptr ? kQuarterTime : previous->start + previous->length();
            header.number = previous == nullptr ? 1 : previous->number + 1;
            header.tempo.quarter_bpm = previous == nullptr ? song.tempo_bpm : previous->tempo.quarter_bpm;
            header.triplet_feel = triplet_feel;
            if (auto step = read_gp1_track_measures(stream, song, header, last_starts, track_count,
                                                    version_code, key_signature);
                !step) {
                return fail_from<Song>(step);
            }
            previous = &song.measure_headers.back();
        }

        return ParseResult<Song>::success(std::move(song));
    }

} // namespace libgp_parser
