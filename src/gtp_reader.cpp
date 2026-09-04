#include "libgp_parser/gtp_reader.hpp"
#include "libgp_parser/binary_reader.hpp"
#include "libgp_parser/bit_constants.hpp"
#include "libgp_parser/channel.hpp"
#include "libgp_parser/flag_constants.hpp"
#include "libgp_parser/version_constants.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>
#include <string_view>
#include <utility>

namespace libgp_parser {
    namespace {

        constexpr float kGpBendSemitone = 25.0F;
        constexpr float kGpBendPosition = 60.0F;

        struct GtpVersion {
            int format;
            int code;
        };

        struct GtpContext {
            int format{0};
            int code{0};
            TripletFeel triplet_feel{TripletFeel::None};
            std::vector<int> key_signatures;
            std::vector<Clef> clefs;
            std::vector<Channel> mixer;
            Tempo tempo{};
        };

        template <typename T> ParseResult<Ok> require(const ParseResult<T> &result) {
            if (!result.has_value()) {
                return ParseResult<Ok>::failure(result.error());
            }
            return ParseResult<Ok>::success(Ok{});
        }

        template <typename To, typename From>
        ParseResult<To> fail_from(const ParseResult<From> &result) {
            return ParseResult<To>::failure(result.error());
        }

        [[nodiscard]] constexpr bool flag_set(const int flags, const int mask) noexcept {
            return (flags & mask) != 0;
        }

        ParseResult<GtpVersion> detect_version(const std::string &version) {
            if (version == "FICHIER GUITAR PRO v3.00") {
                return ParseResult<GtpVersion>::success({.format = Version::Version3, .code = 0});
            }
            if (version == "FICHIER GUITAR PRO v4.00") {
                return ParseResult<GtpVersion>::success({.format = Version::Version4, .code = 0});
            }
            if (version == "FICHIER GUITAR PRO v4.06") {
                return ParseResult<GtpVersion>::success({.format = Version::Version4, .code = 1});
            }
            if (version == "FICHIER GUITAR PRO L4.06") {
                return ParseResult<GtpVersion>::success({.format = Version::Version4, .code = 2});
            }
            if (version == "FICHIER GUITAR PRO v5.00") {
                return ParseResult<GtpVersion>::success({.format = Version::Version5, .code = 0});
            }
            if (version == "FICHIER GUITAR PRO v5.10") {
                return ParseResult<GtpVersion>::success({.format = Version::Version5, .code = 1});
            }
            return ParseResult<GtpVersion>::failure(
                {.code = ParseErrorCode::Unsupported,
                 .message = "Unsupported GTP version: " + version});
        }

        ParseResult<Ok> read_string_field(BinaryReader &stream, std::string &out) {
            auto value = stream.read_string_byte_size_of_integer();
            if (!value) {
                return ParseResult<Ok>::failure(value.error());
            }
            out = std::move(value.value());
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> skip_string_field(BinaryReader &stream) {
            auto value = stream.read_string_byte_size_of_integer();
            if (!value) {
                return ParseResult<Ok>::failure(value.error());
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_info_comments(BinaryReader &stream, SongMetadata &metadata) {
            auto count = stream.read_i32();
            if (!count) {
                return ParseResult<Ok>::failure(count.error());
            }
            metadata.notices.clear();
            for (int i = 0; i < count.value(); ++i) {
                auto comment = stream.read_string_byte_size_of_integer();
                if (!comment) {
                    return ParseResult<Ok>::failure(comment.error());
                }
                metadata.notices += comment.value();
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_info_gp3_gp4(BinaryReader &stream, SongMetadata &metadata) {
            if (auto step = read_string_field(stream, metadata.title); !step) {
                return step;
            }
            if (auto step = skip_string_field(stream); !step) {
                return step;
            }
            if (auto step = read_string_field(stream, metadata.artist); !step) {
                return step;
            }
            if (auto step = read_string_field(stream, metadata.album); !step) {
                return step;
            }
            if (auto step = read_string_field(stream, metadata.words_and_music); !step) {
                return step;
            }
            if (auto step = read_string_field(stream, metadata.copyright); !step) {
                return step;
            }
            if (auto step = read_string_field(stream, metadata.tabber); !step) {
                return step;
            }
            if (auto step = skip_string_field(stream); !step) {
                return step;
            }
            return read_info_comments(stream, metadata);
        }

        ParseResult<Ok> read_info_gp5(BinaryReader &stream, SongMetadata &metadata) {
            if (auto step = read_string_field(stream, metadata.title); !step) {
                return step;
            }
            if (auto step = skip_string_field(stream); !step) {
                return step;
            }
            if (auto step = read_string_field(stream, metadata.artist); !step) {
                return step;
            }
            if (auto step = read_string_field(stream, metadata.album); !step) {
                return step;
            }
            if (auto step = read_string_field(stream, metadata.words_and_music); !step) {
                return step;
            }
            if (auto step = skip_string_field(stream); !step) {
                return step;
            }
            if (auto step = read_string_field(stream, metadata.copyright); !step) {
                return step;
            }
            if (auto step = read_string_field(stream, metadata.tabber); !step) {
                return step;
            }
            if (auto step = skip_string_field(stream); !step) {
                return step;
            }
            return read_info_comments(stream, metadata);
        }

        ParseResult<int> read_key_signature(BinaryReader &stream) {
            auto byte = stream.read_i8();
            if (!byte) {
                return ParseResult<int>::failure(byte.error());
            }
            int key = byte.value();
            if (key < 0) {
                key = 7 - key;
            }
            return ParseResult<int>::success(key);
        }

        int to_channel_short(const std::int8_t byte) {
            const int value = (static_cast<int>(byte) * 8) - 1;
            return std::max(value, 0);
        }

        int to_stroke_value(const int value) {
            if (value == 1 || value == 2) {
                return DurationValue::SixtyFourth;
            }
            if (value == 3) {
                return DurationValue::ThirtySecond;
            }
            if (value == 4) {
                return DurationValue::Sixteenth;
            }
            if (value == 5) {
                return DurationValue::Eighth;
            }
            if (value == 6) {
                return DurationValue::Quarter;
            }
            return DurationValue::SixtyFourth;
        }

        ParseResult<std::vector<Channel>> read_channels(BinaryReader &stream) {
            std::vector<Channel> channels;
            channels.reserve(64);
            for (int i = 0; i < Constants::ShiftByte64; ++i) {
                Channel channel;
                auto program = stream.read_i32();
                if (!program) {
                    return fail_from<std::vector<Channel>>(program);
                }
                channel.program = program.value() < 0 ? 0 : program.value();
                auto volume = stream.read_i8();
                auto balance = stream.read_i8();
                auto chorus = stream.read_i8();
                auto reverb = stream.read_i8();
                auto phaser = stream.read_i8();
                auto tremolo = stream.read_i8();
                if (!volume || !balance || !chorus || !reverb || !phaser || !tremolo) {
                    return ParseResult<std::vector<Channel>>::failure(
                        {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
                }
                channel.volume = to_channel_short(volume.value());
                channel.balance = to_channel_short(balance.value());
                channel.chorus = to_channel_short(chorus.value());
                channel.reverb = to_channel_short(reverb.value());
                channel.phaser = to_channel_short(phaser.value());
                channel.tremolo = to_channel_short(tremolo.value());
                channel.bank = (i == kDefaultPercussionChannel) ? kDefaultPercussionBank : kDefaultBank;
                if (auto step = stream.skip(2); !step) {
                    return fail_from<std::vector<Channel>>(step);
                }
                channels.push_back(std::move(channel));
            }
            return ParseResult<std::vector<Channel>>::success(std::move(channels));
        }

        ParseResult<Lyric> read_lyrics(BinaryReader &stream) {
            Lyric lyric;
            auto from = stream.read_i32();
            if (!from) {
                return fail_from<Lyric>(from);
            }
            lyric.from = from.value();
            auto text = stream.read_string_integer();
            if (!text) {
                return fail_from<Lyric>(text);
            }
            lyric.text = std::move(text.value());
            for (int i = 0; i < 4; ++i) {
                if (auto marker = stream.read_i32(); !marker) {
                    return fail_from<Lyric>(marker);
                }
                if (auto chunk = stream.read_string_integer(); !chunk) {
                    return fail_from<Lyric>(chunk);
                }
            }
            return ParseResult<Lyric>::success(std::move(lyric));
        }

        ParseResult<RgbColor> read_color(BinaryReader &stream) {
            auto red = stream.read_u8();
            auto green = stream.read_u8();
            auto blue = stream.read_u8();
            if (!red || !green || !blue) {
                return ParseResult<RgbColor>::failure(
                    {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
            }
            if (auto step = stream.skip(1); !step) {
                return fail_from<RgbColor>(step);
            }
            return ParseResult<RgbColor>::success(RgbColor{
                .r = red.value(),
                .g = green.value(),
                .b = blue.value(),
            });
        }

        ParseResult<Marker> read_marker(BinaryReader &stream, const int measure) {
            Marker marker;
            marker.measure = measure;
            auto title = stream.read_string_byte_size_of_integer();
            if (!title) {
                return fail_from<Marker>(title);
            }
            marker.title = std::move(title.value());
            auto color = read_color(stream);
            if (!color) {
                return fail_from<Marker>(color);
            }
            marker.color = color.value();
            return ParseResult<Marker>::success(std::move(marker));
        }

        int parse_repeat_alternative(const Song &song, const int measure, const int value) {
            int alternative = 0;
            int existent = 0;
            for (const MeasureHeader &header : song.measure_headers) {
                if (header.number == measure) {
                    break;
                }
                if (header.repeat_open) {
                    existent = 0;
                }
                existent |= header.repeat_alternative;
            }
            for (int i = 0; i < 8; ++i) {
                if (value > i && (existent & (1 << i)) == 0) {
                    alternative |= (1 << i);
                }
            }
            return alternative;
        }

        ParseResult<Duration> read_duration(BinaryReader &stream, const int flags) {
            Duration duration;
            auto signed_value = stream.read_i8();
            if (!signed_value) {
                return fail_from<Duration>(signed_value);
            }
            duration.value = 1 << (signed_value.value() + 2);
            duration.dotted = flag_set(flags, 0x01);
            if (flag_set(flags, 0x20)) {
                auto division = stream.read_i32();
                if (!division) {
                    return fail_from<Duration>(division);
                }
                switch (division.value()) {
                case 3:
                    duration.division.enters = 3;
                    duration.division.times = 2;
                    break;
                case 5:
                    duration.division.enters = 5;
                    duration.division.times = 4;
                    break;
                case 6:
                    duration.division.enters = 6;
                    duration.division.times = 4;
                    break;
                case 7:
                    duration.division.enters = 7;
                    duration.division.times = 4;
                    break;
                case 9:
                    duration.division.enters = 9;
                    duration.division.times = 8;
                    break;
                case 10:
                    duration.division.enters = 10;
                    duration.division.times = 8;
                    break;
                case 11:
                    duration.division.enters = 11;
                    duration.division.times = 8;
                    break;
                case 12:
                    duration.division.enters = 12;
                    duration.division.times = 8;
                    break;
                case 13:
                    duration.division.enters = 13;
                    duration.division.times = 8;
                    break;
                default:
                    break;
                }
            }
            return ParseResult<Duration>::success(duration);
        }

        ParseResult<Ok> read_bend(BinaryReader &stream, NoteEffect &effect) {
            if (auto step = stream.skip(5); !step) {
                return step;
            }
            EffectBend bend;
            auto points = stream.read_i32();
            if (!points) {
                return fail_from<Ok>(points);
            }
            for (int i = 0; i < points.value(); ++i) {
                auto position = stream.read_i32();
                auto value = stream.read_i32();
                if (!position || !value) {
                    return ParseResult<Ok>::failure(
                        {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
                }
                if (auto vibrato = stream.read_i8(); !vibrato) {
                    return fail_from<Ok>(vibrato);
                }
                bend.points.push_back(
                    {.position = static_cast<int>(std::lround(position.value() *
                                                              EffectBend::MaxPositionLength /
                                                              kGpBendPosition)),
                     .value = static_cast<int>(std::lround(value.value() * EffectBend::SemitoneLength /
                                                           kGpBendSemitone))});
            }
            if (!bend.empty()) {
                effect.bend = std::move(bend);
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_tremolo_bar_points(BinaryReader &stream, NoteEffect &effect) {
            if (auto step = stream.skip(5); !step) {
                return step;
            }
            EffectTremoloBar bar;
            auto points = stream.read_i32();
            if (!points) {
                return fail_from<Ok>(points);
            }
            for (int i = 0; i < points.value(); ++i) {
                auto position = stream.read_i32();
                auto value = stream.read_i32();
                if (!position || !value) {
                    return ParseResult<Ok>::failure(
                        {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
                }
                if (auto extra = stream.read_i8(); !extra) {
                    return fail_from<Ok>(extra);
                }
                bar.points.push_back(
                    {.position = static_cast<int>(std::lround(position.value() *
                                                              EffectTremoloBar::MaxPositionLength /
                                                              kGpBendPosition)),
                     .value = static_cast<int>(
                         std::lround(value.value() / (kGpBendSemitone * 2.0F)))});
            }
            if (!bar.empty()) {
                effect.tremolo_bar = std::move(bar);
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_grace_gp34(BinaryReader &stream, NoteEffect &effect) {
            auto fret = stream.read_u8();
            auto dynamic = stream.read_u8();
            auto transition = stream.read_u8();
            auto duration = stream.read_u8();
            if (!fret || !dynamic || !transition || !duration) {
                return ParseResult<Ok>::failure(
                    {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
            }
            EffectGrace grace;
            grace.dead = fret.value() == 255;
            grace.fret = grace.dead ? 0 : fret.value();
            grace.on_beat = false;
            grace.dynamic = (Velocity::Min + (Velocity::Increment * dynamic.value())) - Velocity::Increment;
            grace.duration = duration.value();
            grace.transition = static_cast<GraceTransition>(transition.value());
            effect.grace = grace;
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_grace_gp5(BinaryReader &stream, NoteEffect &effect) {
            auto fret = stream.read_u8();
            auto dynamic = stream.read_u8();
            auto transition = stream.read_i8();
            auto duration = stream.read_u8();
            auto flags = stream.read_u8();
            if (!fret || !dynamic || !transition || !duration || !flags) {
                return ParseResult<Ok>::failure(
                    {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
            }
            EffectGrace grace;
            grace.fret = fret.value();
            grace.dynamic = (Velocity::Min + (Velocity::Increment * dynamic.value())) - Velocity::Increment;
            grace.duration = duration.value();
            grace.dead = (flags.value() & 0x01) != 0;
            grace.on_beat = (flags.value() & 0x02) != 0;
            grace.transition = static_cast<GraceTransition>(transition.value());
            effect.grace = grace;
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_tremolo_picking(BinaryReader &stream, NoteEffect &effect) {
            auto value = stream.read_u8();
            if (!value) {
                return fail_from<Ok>(value);
            }
            EffectTremoloPicking picking;
            if (value.value() == 1) {
                picking.duration.value = DurationValue::Eighth;
                effect.tremolo_picking = picking;
            } else if (value.value() == 2) {
                picking.duration.value = DurationValue::Sixteenth;
                effect.tremolo_picking = picking;
            } else if (value.value() == 3) {
                picking.duration.value = DurationValue::ThirtySecond;
                effect.tremolo_picking = picking;
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_trill(BinaryReader &stream, NoteEffect &effect) {
            auto fret = stream.read_i8();
            auto period = stream.read_i8();
            if (!fret || !period) {
                return ParseResult<Ok>::failure(
                    {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
            }
            EffectTrill trill;
            trill.fret = fret.value();
            if (period.value() == 1) {
                trill.duration.value = DurationValue::Sixteenth;
                effect.trill = trill;
            } else if (period.value() == 2) {
                trill.duration.value = DurationValue::ThirtySecond;
                effect.trill = trill;
            } else if (period.value() == 3) {
                trill.duration.value = DurationValue::SixtyFourth;
                effect.trill = trill;
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_artificial_harmonic_gp5(BinaryReader &stream, NoteEffect &effect,
                                                     const int note_fret) {
            auto type = stream.read_i8();
            if (!type) {
                return fail_from<Ok>(type);
            }
            EffectHarmonic harmonic;
            if (type.value() == 1) {
                harmonic.type = HarmonicType::Natural;
                effect.harmonic = harmonic;
            } else if (type.value() == 2) {
                if (auto step = stream.skip(3); !step) {
                    return step;
                }
                harmonic.type = HarmonicType::Artificial;
                effect.harmonic = harmonic;
            } else if (type.value() == 3) {
                harmonic.type = HarmonicType::Tapped;
                auto right_hand = stream.read_i8();
                if (!right_hand) {
                    return fail_from<Ok>(right_hand);
                }
                for (int i = 0; i < 6; ++i) {
                    if (right_hand.value() - note_fret == kNaturalFrequencies[i][0]) {
                        harmonic.data = i;
                    }
                }
                effect.harmonic = harmonic;
            } else if (type.value() == 4) {
                harmonic.type = HarmonicType::Pinch;
                effect.harmonic = harmonic;
            } else if (type.value() == 5) {
                harmonic.type = HarmonicType::Semi;
                effect.harmonic = harmonic;
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> skip_if_non_negative(BinaryReader &stream, const int value) {
            if (value < 0) {
                return ParseResult<Ok>::success(Ok{});
            }
            return require(stream.read_i8());
        }

        ParseResult<Ok> read_mix_change(BinaryReader &stream, GtpContext &ctx) {
            if (auto instrument = stream.read_i8(); !instrument) {
                return fail_from<Ok>(instrument);
            }
            if (ctx.format == Version::Version5) {
                if (auto step = stream.skip(16); !step) {
                    return step;
                }
            }
            auto volume = stream.read_i8();
            auto pan = stream.read_i8();
            auto chorus = stream.read_i8();
            auto reverb = stream.read_i8();
            auto phaser = stream.read_i8();
            auto tremolo = stream.read_i8();
            if (!volume || !pan || !chorus || !reverb || !phaser || !tremolo) {
                return ParseResult<Ok>::failure(
                    {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
            }
            if (ctx.format == Version::Version5) {
                if (auto name = stream.read_string_byte_size_of_integer(); !name) {
                    return fail_from<Ok>(name);
                }
            }
            auto tempo_value = stream.read_i32();
            if (!tempo_value) {
                return fail_from<Ok>(tempo_value);
            }
            if (auto step = skip_if_non_negative(stream, volume.value()); !step) {
                return step;
            }
            if (auto step = skip_if_non_negative(stream, pan.value()); !step) {
                return step;
            }
            if (auto step = skip_if_non_negative(stream, chorus.value()); !step) {
                return step;
            }
            if (auto step = skip_if_non_negative(stream, reverb.value()); !step) {
                return step;
            }
            if (auto step = skip_if_non_negative(stream, phaser.value()); !step) {
                return step;
            }
            if (auto step = skip_if_non_negative(stream, tremolo.value()); !step) {
                return step;
            }
            if (tempo_value.value() >= 0) {
                ctx.tempo.quarter_bpm = tempo_value.value();
                if (auto step = stream.skip(1); !step) {
                    return step;
                }
                if (ctx.format == Version::Version5 && ctx.code > 0) {
                    if (auto step = stream.skip(1); !step) {
                        return step;
                    }
                }
            }
            if (ctx.format >= Version::Version4) {
                if (auto extra = stream.read_i8(); !extra) {
                    return fail_from<Ok>(extra);
                }
            }
            if (ctx.format == Version::Version5) {
                if (auto step = stream.skip(1); !step) {
                    return step;
                }
                if (ctx.code > 0) {
                    if (auto a = stream.read_string_byte_size_of_integer(); !a) {
                        return fail_from<Ok>(a);
                    }
                    if (auto b = stream.read_string_byte_size_of_integer(); !b) {
                        return fail_from<Ok>(b);
                    }
                }
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_chord_gp5(BinaryReader &stream, Beat &beat, const int strings) {
            if (auto step = stream.skip(17); !step) {
                return step;
            }
            Chord chord;
            auto name = stream.read_string_byte(21);
            if (!name) {
                return fail_from<Ok>(name);
            }
            chord.name = std::move(name.value());
            if (auto step = stream.skip(4); !step) {
                return step;
            }
            auto first = stream.read_i32();
            if (!first) {
                return fail_from<Ok>(first);
            }
            chord.first_fret = first.value();
            chord.frets.assign(static_cast<std::size_t>(strings), -1);
            for (int i = 0; i < 7; ++i) {
                auto fret = stream.read_i32();
                if (!fret) {
                    return fail_from<Ok>(fret);
                }
                if (i < strings) {
                    chord.frets[static_cast<std::size_t>(i)] = fret.value();
                }
            }
            if (auto step = stream.skip(32); !step) {
                return step;
            }
            if (chord.note_count() > 0) {
                beat.chord = std::move(chord);
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_chord_gp4(BinaryReader &stream, Beat &beat, const int strings) {
            auto header = stream.read_u8();
            if (!header) {
                return fail_from<Ok>(header);
            }
            Chord chord;
            chord.frets.assign(static_cast<std::size_t>(strings), -1);
            if ((header.value() & 0x01) == 0) {
                auto name = stream.read_string_byte_size_of_integer();
                if (!name) {
                    return fail_from<Ok>(name);
                }
                chord.name = std::move(name.value());
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
            } else {
                if (auto step = stream.skip(16); !step) {
                    return step;
                }
                auto name = stream.read_string_byte(21);
                if (!name) {
                    return fail_from<Ok>(name);
                }
                chord.name = std::move(name.value());
                if (auto step = stream.skip(4); !step) {
                    return step;
                }
                auto first = stream.read_i32();
                if (!first) {
                    return fail_from<Ok>(first);
                }
                chord.first_fret = first.value();
                for (int i = 0; i < 7; ++i) {
                    auto fret = stream.read_i32();
                    if (!fret) {
                        return fail_from<Ok>(fret);
                    }
                    if (i < strings) {
                        chord.frets[static_cast<std::size_t>(i)] = fret.value();
                    }
                }
                if (auto step = stream.skip(32); !step) {
                    return step;
                }
            }
            if (chord.note_count() > 0) {
                beat.chord = std::move(chord);
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_chord_gp3(BinaryReader &stream, Beat &beat, const int strings) {
            auto header = stream.read_u8();
            if (!header) {
                return fail_from<Ok>(header);
            }
            Chord chord;
            chord.frets.assign(static_cast<std::size_t>(strings), -1);
            if ((header.value() & 0x01) == 0) {
                auto name = stream.read_string_byte_size_of_integer();
                if (!name) {
                    return fail_from<Ok>(name);
                }
                chord.name = std::move(name.value());
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
            } else {
                if (auto step = stream.skip(25); !step) {
                    return step;
                }
                auto name = stream.read_string_byte(34);
                if (!name) {
                    return fail_from<Ok>(name);
                }
                chord.name = std::move(name.value());
                auto first = stream.read_i32();
                if (!first) {
                    return fail_from<Ok>(first);
                }
                chord.first_fret = first.value();
                for (int i = 0; i < 6; ++i) {
                    auto fret = stream.read_i32();
                    if (!fret) {
                        return fail_from<Ok>(fret);
                    }
                    if (i < strings) {
                        chord.frets[static_cast<std::size_t>(i)] = fret.value();
                    }
                }
                if (auto step = stream.skip(36); !step) {
                    return step;
                }
            }
            if (chord.note_count() > 0) {
                beat.chord = std::move(chord);
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_chord(BinaryReader &stream, Beat &beat, const int strings,
                                   const int format) {
            if (format == Version::Version5) {
                return read_chord_gp5(stream, beat, strings);
            }
            if (format == Version::Version4) {
                return read_chord_gp4(stream, beat, strings);
            }
            return read_chord_gp3(stream, beat, strings);
        }

        ParseResult<Ok> read_text(BinaryReader &stream, Beat &beat) {
            auto text = stream.read_string_byte_size_of_integer();
            if (!text) {
                return fail_from<Ok>(text);
            }
            beat.text = std::move(text.value());
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_beat_effects_gp5(BinaryReader &stream, Beat &beat, NoteEffect &effect) {
            auto flags1 = stream.read_u8();
            auto flags2 = stream.read_u8();
            if (!flags1 || !flags2) {
                return ParseResult<Ok>::failure(
                    {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
            }
            effect.fade_in = flag_set(flags1.value(), 0x10);
            effect.vibrato = flag_set(flags1.value(), 0x02);
            if (flag_set(flags1.value(), 0x20)) {
                auto kind = stream.read_u8();
                if (!kind) {
                    return fail_from<Ok>(kind);
                }
                effect.tapping = kind.value() == 1;
                effect.slapping = kind.value() == 2;
                effect.popping = kind.value() == 3;
            }
            if (flag_set(flags2.value(), 0x04)) {
                if (auto step = read_tremolo_bar_points(stream, effect); !step) {
                    return step;
                }
            }
            if (flag_set(flags1.value(), 0x40)) {
                auto up = stream.read_i8();
                auto down = stream.read_i8();
                if (!up || !down) {
                    return ParseResult<Ok>::failure(
                        {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
                }
                if (up.value() > 0) {
                    beat.stroke.direction = StrokeDirection::Up;
                    beat.stroke.value = to_stroke_value(up.value());
                } else if (down.value() > 0) {
                    beat.stroke.direction = StrokeDirection::Down;
                    beat.stroke.value = to_stroke_value(down.value());
                }
            }
            if (flag_set(flags2.value(), 0x02)) {
                auto direction = stream.read_i8();
                if (!direction) {
                    return fail_from<Ok>(direction);
                }
                if ((direction.value() & 0x01) != 0) {
                    beat.pick_stroke.direction = StrokeDirection::Up;
                } else if ((direction.value() & 0x02) != 0) {
                    beat.pick_stroke.direction = StrokeDirection::Down;
                }
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_beat_effects_gp4(BinaryReader &stream, Beat &beat, NoteEffect &effect) {
            auto flags1 = stream.read_u8();
            auto flags2 = stream.read_u8();
            if (!flags1 || !flags2) {
                return ParseResult<Ok>::failure(
                    {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
            }
            effect.fade_in = flag_set(flags1.value(), 0x10);
            effect.vibrato = flag_set(flags1.value(), 0x02);
            if (flag_set(flags1.value(), 0x20)) {
                auto kind = stream.read_u8();
                if (!kind) {
                    return fail_from<Ok>(kind);
                }
                effect.tapping = kind.value() == 1;
                effect.slapping = kind.value() == 2;
                effect.popping = kind.value() == 3;
            }
            if (flag_set(flags2.value(), 0x04)) {
                if (auto step = read_tremolo_bar_points(stream, effect); !step) {
                    return step;
                }
            }
            if (flag_set(flags1.value(), 0x40)) {
                auto down = stream.read_i8();
                auto up = stream.read_i8();
                if (!down || !up) {
                    return ParseResult<Ok>::failure(
                        {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
                }
                if (down.value() > 0) {
                    beat.stroke.direction = StrokeDirection::Down;
                    beat.stroke.value = to_stroke_value(down.value());
                } else if (up.value() > 0) {
                    beat.stroke.direction = StrokeDirection::Up;
                    beat.stroke.value = to_stroke_value(up.value());
                }
            }
            if (flag_set(flags2.value(), 0x02)) {
                auto direction = stream.read_i8();
                if (!direction) {
                    return fail_from<Ok>(direction);
                }
                if ((direction.value() & 0x01) != 0) {
                    beat.pick_stroke.direction = StrokeDirection::Up;
                } else if ((direction.value() & 0x02) != 0) {
                    beat.pick_stroke.direction = StrokeDirection::Down;
                }
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_beat_effects_gp3(BinaryReader &stream, Beat &beat, NoteEffect &effect) {
            auto flags = stream.read_u8();
            if (!flags) {
                return fail_from<Ok>(flags);
            }
            effect.vibrato = flag_set(flags.value(), 0x01) || flag_set(flags.value(), 0x02);
            effect.fade_in = flag_set(flags.value(), 0x10);
            if (flag_set(flags.value(), 0x20)) {
                auto type = stream.read_u8();
                if (!type) {
                    return fail_from<Ok>(type);
                }
                if (type.value() == 0) {
                    auto value = stream.read_i32();
                    if (!value) {
                        return fail_from<Ok>(value);
                    }
                    EffectTremoloBar bar;
                    bar.points.push_back({.position = 0, .value = 0});
                    bar.points.push_back(
                        {.position = static_cast<int>(
                             std::lround(EffectTremoloBar::MaxPositionLength / 2.0F)),
                         .value = static_cast<int>(
                             std::lround(-(value.value() / (kGpBendSemitone * 2.0F))))});
                    bar.points.push_back({.position = EffectTremoloBar::MaxPositionLength, .value = 0});
                    effect.tremolo_bar = bar;
                } else {
                    effect.tapping = type.value() == 1;
                    effect.slapping = type.value() == 2;
                    effect.popping = type.value() == 3;
                    if (auto extra = stream.read_i32(); !extra) {
                        return fail_from<Ok>(extra);
                    }
                }
            }
            if (flag_set(flags.value(), 0x40)) {
                auto down = stream.read_i8();
                auto up = stream.read_i8();
                if (!down || !up) {
                    return ParseResult<Ok>::failure(
                        {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
                }
                if (down.value() > 0) {
                    beat.stroke.direction = StrokeDirection::Down;
                    beat.stroke.value = to_stroke_value(down.value());
                } else if (up.value() > 0) {
                    beat.stroke.direction = StrokeDirection::Up;
                    beat.stroke.value = to_stroke_value(up.value());
                }
            }
            if (flag_set(flags.value(), 0x04)) {
                effect.harmonic = EffectHarmonic{.type = HarmonicType::Natural};
            }
            if (flag_set(flags.value(), 0x08)) {
                effect.harmonic = EffectHarmonic{.type = HarmonicType::Artificial};
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_note_effects_gp5(BinaryReader &stream, NoteEffect &effect,
                                              const int note_fret) {
            auto flags1 = stream.read_u8();
            auto flags2 = stream.read_u8();
            if (!flags1 || !flags2) {
                return ParseResult<Ok>::failure(
                    {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
            }
            if (flag_set(flags1.value(), 0x01)) {
                if (auto step = read_bend(stream, effect); !step) {
                    return step;
                }
            }
            if (flag_set(flags1.value(), 0x10)) {
                if (auto step = read_grace_gp5(stream, effect); !step) {
                    return step;
                }
            }
            if (flag_set(flags2.value(), 0x04)) {
                if (auto step = read_tremolo_picking(stream, effect); !step) {
                    return step;
                }
            }
            if (flag_set(flags2.value(), 0x08)) {
                effect.slide = true;
                if (auto extra = stream.read_i8(); !extra) {
                    return fail_from<Ok>(extra);
                }
            }
            if (flag_set(flags2.value(), 0x10)) {
                if (auto step = read_artificial_harmonic_gp5(stream, effect, note_fret); !step) {
                    return step;
                }
            }
            if (flag_set(flags2.value(), 0x20)) {
                if (auto step = read_trill(stream, effect); !step) {
                    return step;
                }
            }
            effect.hammer = flag_set(flags1.value(), 0x02);
            effect.let_ring = flag_set(flags1.value(), 0x08);
            effect.vibrato = flag_set(flags2.value(), 0x40) || effect.vibrato;
            effect.palm_mute = flag_set(flags2.value(), 0x02);
            effect.staccato = flag_set(flags2.value(), 0x01);
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_note_effects_gp4(BinaryReader &stream, NoteEffect &effect) {
            auto flags1 = stream.read_u8();
            auto flags2 = stream.read_u8();
            if (!flags1 || !flags2) {
                return ParseResult<Ok>::failure(
                    {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
            }
            effect.hammer = flag_set(flags1.value(), 0x02);
            effect.let_ring = flag_set(flags1.value(), 0x08);
            effect.vibrato = flag_set(flags2.value(), 0x40) || effect.vibrato;
            effect.palm_mute = flag_set(flags2.value(), 0x02);
            effect.staccato = flag_set(flags2.value(), 0x01);
            if (flag_set(flags1.value(), 0x01)) {
                if (auto step = read_bend(stream, effect); !step) {
                    return step;
                }
            }
            if (flag_set(flags1.value(), 0x10)) {
                if (auto step = read_grace_gp34(stream, effect); !step) {
                    return step;
                }
            }
            if (flag_set(flags2.value(), 0x04)) {
                if (auto step = read_tremolo_picking(stream, effect); !step) {
                    return step;
                }
            }
            if (flag_set(flags2.value(), 0x08)) {
                effect.slide = true;
                if (auto extra = stream.read_i8(); !extra) {
                    return fail_from<Ok>(extra);
                }
            }
            if (flag_set(flags2.value(), 0x10)) {
                auto type = stream.read_i8();
                if (!type) {
                    return fail_from<Ok>(type);
                }
                EffectHarmonic harmonic;
                if (type.value() == 1) {
                    harmonic.type = HarmonicType::Natural;
                } else if (type.value() == 3) {
                    harmonic.type = HarmonicType::Tapped;
                } else if (type.value() == 4) {
                    harmonic.type = HarmonicType::Pinch;
                } else if (type.value() == 5) {
                    harmonic.type = HarmonicType::Semi;
                } else if (type.value() == 15) {
                    harmonic.type = HarmonicType::Artificial;
                    harmonic.data = 2;
                } else if (type.value() == 17) {
                    harmonic.type = HarmonicType::Artificial;
                    harmonic.data = 3;
                } else if (type.value() == 22) {
                    harmonic.type = HarmonicType::Artificial;
                    harmonic.data = 0;
                }
                effect.harmonic = harmonic;
            }
            if (flag_set(flags2.value(), 0x20)) {
                if (auto step = read_trill(stream, effect); !step) {
                    return step;
                }
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_note_effects_gp3(BinaryReader &stream, NoteEffect &effect) {
            auto flags = stream.read_u8();
            if (!flags) {
                return fail_from<Ok>(flags);
            }
            effect.hammer = flag_set(flags.value(), 0x02);
            effect.slide = flag_set(flags.value(), 0x04);
            effect.let_ring = flag_set(flags.value(), 0x08);
            if (flag_set(flags.value(), 0x01)) {
                if (auto step = read_bend(stream, effect); !step) {
                    return step;
                }
            }
            if (flag_set(flags.value(), 0x10)) {
                if (auto step = read_grace_gp34(stream, effect); !step) {
                    return step;
                }
            }
            return ParseResult<Ok>::success(Ok{});
        }

        int tied_note_value(const int string, const Track &track) {
            for (int m = static_cast<int>(track.measures.size()) - 1; m >= 0; --m) {
                const Measure &measure = track.measures[static_cast<std::size_t>(m)];
                for (int b = static_cast<int>(measure.beats.size()) - 1; b >= 0; --b) {
                    const Beat &beat = measure.beats[static_cast<std::size_t>(b)];
                    for (const Voice &voice : beat.voices) {
                        if (voice.empty) {
                            continue;
                        }
                        for (const Note &note : voice.notes) {
                            if (note.string == string) {
                                return note.value;
                            }
                        }
                    }
                }
            }
            return -1;
        }

        ParseResult<Note> read_note(BinaryReader &stream, const Track &track, const int string,
                                    NoteEffect effect, const int format) {
            auto flags = stream.read_u8();
            if (!flags) {
                return fail_from<Note>(flags);
            }
            Note note;
            note.string = string;
            note.effect = std::move(effect);
            note.effect.accentuated_note = flag_set(flags.value(), 0x40);
            if (format == Version::Version5) {
                note.effect.heavy_accentuated_note = flag_set(flags.value(), 0x02);
            }
            note.effect.ghost_note = flag_set(flags.value(), 0x04);
            if (flag_set(flags.value(), 0x20)) {
                auto type = stream.read_u8();
                if (!type) {
                    return fail_from<Note>(type);
                }
                note.tied_note = type.value() == 0x02;
                note.effect.dead_note = type.value() == 0x03;
            }
            if (format < Version::Version5 && flag_set(flags.value(), 0x01)) {
                if (auto step = stream.skip(2); !step) {
                    return fail_from<Note>(step);
                }
            }
            if (flag_set(flags.value(), 0x10)) {
                auto velocity = stream.read_i8();
                if (!velocity) {
                    return fail_from<Note>(velocity);
                }
                note.velocity =
                    (Velocity::Min + (Velocity::Increment * velocity.value())) - Velocity::Increment;
            }
            if (flag_set(flags.value(), 0x20)) {
                auto fret = stream.read_i8();
                if (!fret) {
                    return fail_from<Note>(fret);
                }
                const int value = note.tied_note ? tied_note_value(string, track) : fret.value();
                note.value = (value >= 0 && value < 100) ? value : 0;
            }
            if (flag_set(flags.value(), 0x80)) {
                if (auto step = stream.skip(2); !step) {
                    return fail_from<Note>(step);
                }
            }
            if (format == Version::Version5) {
                if (flag_set(flags.value(), 0x01)) {
                    if (auto step = stream.skip(8); !step) {
                        return fail_from<Note>(step);
                    }
                }
                if (auto extra = stream.skip(1); !extra) {
                    return fail_from<Note>(extra);
                }
            }
            if (flag_set(flags.value(), 0x08)) {
                ParseResult<Ok> effects =
                    format == Version::Version5
                        ? read_note_effects_gp5(stream, note.effect, note.value)
                    : format == Version::Version4 ? read_note_effects_gp4(stream, note.effect)
                                                  : read_note_effects_gp3(stream, note.effect);
                if (!effects) {
                    return fail_from<Note>(effects);
                }
            }
            return ParseResult<Note>::success(std::move(note));
        }

        ParseResult<long> read_beat(BinaryReader &stream, Measure &measure, const Track &track,
                                    GtpContext &ctx, const long start, const int voice_index) {
            auto flags = stream.read_u8();
            if (!flags) {
                return fail_from<long>(flags);
            }
            Beat *beat = nullptr;
            if (ctx.format == Version::Version5) {
                beat = &get_or_create_beat(measure, start);
            } else {
                Beat created;
                created.start = start;
                measure.beats.push_back(std::move(created));
                beat = &measure.beats.back();
            }
            Voice &voice = beat->voice(ctx.format == Version::Version5 ? voice_index : 0);
            if (flag_set(flags.value(), 0x40)) {
                auto beat_type = stream.read_u8();
                if (!beat_type) {
                    return fail_from<long>(beat_type);
                }
                if (ctx.format == Version::Version5) {
                    voice.empty = (beat_type.value() & 0x02) == 0;
                }
            }
            auto duration = read_duration(stream, flags.value());
            if (!duration) {
                return fail_from<long>(duration);
            }
            NoteEffect effect;
            if (flag_set(flags.value(), 0x02)) {
                if (auto step = read_chord(stream, *beat, track.string_count(), ctx.format); !step) {
                    return fail_from<long>(step);
                }
            }
            if (flag_set(flags.value(), 0x04)) {
                if (auto step = read_text(stream, *beat); !step) {
                    return fail_from<long>(step);
                }
            }
            if (flag_set(flags.value(), 0x08)) {
                ParseResult<Ok> beats =
                    ctx.format == Version::Version5
                        ? read_beat_effects_gp5(stream, *beat, effect)
                    : ctx.format == Version::Version4 ? read_beat_effects_gp4(stream, *beat, effect)
                                                      : read_beat_effects_gp3(stream, *beat, effect);
                if (!beats) {
                    return fail_from<long>(beats);
                }
            }
            if (flag_set(flags.value(), 0x10)) {
                if (auto step = read_mix_change(stream, ctx); !step) {
                    return fail_from<long>(step);
                }
            }
            auto string_flags = stream.read_u8();
            if (!string_flags) {
                return fail_from<long>(string_flags);
            }
            for (int i = 6; i >= 0; --i) {
                if ((string_flags.value() & (1 << i)) != 0 && (6 - i) < track.string_count()) {
                    const int string_number = (6 - i) + 1;
                    auto note = read_note(stream, track, string_number, effect, ctx.format);
                    if (!note) {
                        return fail_from<long>(note);
                    }
                    voice.add_note(std::move(note.value()));
                }
                if (ctx.format == Version::Version5) {
                    voice.duration = duration.value();
                }
            }
            if (ctx.format != Version::Version5) {
                voice.empty = false;
                voice.duration = duration.value();
            }
            if (ctx.format == Version::Version5) {
                if (auto skip_byte = stream.skip(1); !skip_byte) {
                    return fail_from<long>(skip_byte);
                }
                auto read = stream.read_i8();
                if (!read) {
                    return fail_from<long>(read);
                }
                if ((read.value() & 0x08) != 0) {
                    if (auto extra = stream.skip(1); !extra) {
                        return fail_from<long>(extra);
                    }
                }
                return ParseResult<long>::success(voice.empty ? 0 : duration.value().time());
            }
            return ParseResult<long>::success(voice.empty ? 0 : duration.value().time());
        }

        ParseResult<MeasureHeader> read_measure_header_gp5(BinaryReader &stream, const int index,
                                                           TimeSignature &time_signature,
                                                           GtpContext &ctx) {
            auto flags = stream.read_u8();
            if (!flags) {
                return fail_from<MeasureHeader>(flags);
            }
            MeasureHeader header;
            header.number = index + 1;
            header.start = 0;
            header.repeat_open = flag_set(flags.value(), FileFlag::HAS_REPEAT_OPEN);
            if (flag_set(flags.value(), FileFlag::HAS_TIME_NUMERATOR)) {
                auto num = stream.read_i8();
                if (!num) {
                    return fail_from<MeasureHeader>(num);
                }
                time_signature.numerator = num.value();
            }
            if (flag_set(flags.value(), FileFlag::HAS_TIME_DENOMINATOR)) {
                auto den = stream.read_i8();
                if (!den) {
                    return fail_from<MeasureHeader>(den);
                }
                time_signature.denominator.value = den.value();
            }
            header.time_signature = time_signature;
            if (flag_set(flags.value(), FileFlag::HAS_REPEAT_CLOSE)) {
                auto close = stream.read_i8();
                if (!close) {
                    return fail_from<MeasureHeader>(close);
                }
                header.repeat_close = (static_cast<int>(close.value()) & 0xff) - 1;
            }
            if (flag_set(flags.value(), FileFlag::HAS_MARKER)) {
                auto marker = read_marker(stream, header.number);
                if (!marker) {
                    return fail_from<MeasureHeader>(marker);
                }
                header.marker = marker.value();
            }
            if (flag_set(flags.value(), FileFlag::HAS_KEY_SIGNATURE)) {
                auto key = read_key_signature(stream);
                if (!key) {
                    return fail_from<MeasureHeader>(key);
                }
                ctx.key_signatures[static_cast<std::size_t>(index)] = key.value();
                if (auto step = stream.skip(1); !step) {
                    return fail_from<MeasureHeader>(step);
                }
            }
            if (flag_set(flags.value(), FileFlag::HAS_TIME_NUMERATOR) ||
                flag_set(flags.value(), FileFlag::HAS_TIME_DENOMINATOR)) {
                if (auto step = stream.skip(4); !step) {
                    return fail_from<MeasureHeader>(step);
                }
            }
            if (flag_set(flags.value(), FileFlag::HAS_REPEAT_ALTERNATIVE)) {
                auto alt = stream.read_u8();
                if (!alt) {
                    return fail_from<MeasureHeader>(alt);
                }
                header.repeat_alternative = alt.value();
            } else if (auto skip_alt = stream.skip(1); !skip_alt) {
                return fail_from<MeasureHeader>(skip_alt);
            }
            auto feel = stream.read_i8();
            if (!feel) {
                return fail_from<MeasureHeader>(feel);
            }
            if (feel.value() == 1) {
                header.triplet_feel = TripletFeel::Eighth;
            } else if (feel.value() == 2) {
                header.triplet_feel = TripletFeel::Sixteenth;
            }
            return ParseResult<MeasureHeader>::success(std::move(header));
        }

        ParseResult<MeasureHeader> read_measure_header_gp34(BinaryReader &stream, const int number,
                                                            Song &song, TimeSignature &time_signature,
                                                            GtpContext &ctx) {
            auto flags = stream.read_u8();
            if (!flags) {
                return fail_from<MeasureHeader>(flags);
            }
            MeasureHeader header;
            header.number = number;
            header.triplet_feel = ctx.triplet_feel;
            header.repeat_open = flag_set(flags.value(), FileFlag::HAS_REPEAT_OPEN);
            if (flag_set(flags.value(), FileFlag::HAS_TIME_NUMERATOR)) {
                auto num = stream.read_i8();
                if (!num) {
                    return fail_from<MeasureHeader>(num);
                }
                time_signature.numerator = num.value();
            }
            if (flag_set(flags.value(), FileFlag::HAS_TIME_DENOMINATOR)) {
                auto den = stream.read_i8();
                if (!den) {
                    return fail_from<MeasureHeader>(den);
                }
                time_signature.denominator.value = den.value();
            }
            header.time_signature = time_signature;
            if (flag_set(flags.value(), FileFlag::HAS_REPEAT_CLOSE)) {
                auto close = stream.read_i8();
                if (!close) {
                    return fail_from<MeasureHeader>(close);
                }
                header.repeat_close = close.value();
            }
            if (flag_set(flags.value(), FileFlag::HAS_REPEAT_ALTERNATIVE)) {
                auto alt = stream.read_u8();
                if (!alt) {
                    return fail_from<MeasureHeader>(alt);
                }
                header.repeat_alternative = parse_repeat_alternative(song, number, alt.value());
            }
            if (flag_set(flags.value(), FileFlag::HAS_MARKER)) {
                auto marker = read_marker(stream, number);
                if (!marker) {
                    return fail_from<MeasureHeader>(marker);
                }
                header.marker = marker.value();
            }
            if (flag_set(flags.value(), FileFlag::HAS_KEY_SIGNATURE)) {
                auto key = read_key_signature(stream);
                if (!key) {
                    return fail_from<MeasureHeader>(key);
                }
                ctx.key_signatures[static_cast<std::size_t>(number - 1)] = key.value();
                if (auto step = stream.skip(1); !step) {
                    return fail_from<MeasureHeader>(step);
                }
            }
            return ParseResult<MeasureHeader>::success(std::move(header));
        }

        void apply_track_channel(Song &song, Track &track, const int gm1, const int gm2,
                                 const std::vector<Channel> &mixer) {
            if (gm1 < 0 || gm1 >= static_cast<int>(mixer.size())) {
                return;
            }
            Channel channel = mixer[static_cast<std::size_t>(gm1)];
            const std::string gm1_s = std::to_string(gm1);
            const std::string gm2_s =
                std::to_string(gm1 != kDefaultPercussionChannel ? gm2 : gm1);
            for (const Channel &existing : song.channels) {
                if (const ChannelParameter *param = find_parameter(existing, kGmChannel1Key)) {
                    if (param->value == gm1_s) {
                        channel.channel_id = existing.channel_id;
                    }
                }
            }
            if (channel.channel_id <= 0) {
                channel.channel_id = static_cast<int>(song.channels.size()) + 1;
                set_parameter(channel, kGmChannel1Key, gm1_s);
                set_parameter(channel, kGmChannel2Key, gm2_s);
                song.channels.push_back(channel);
            }
            track.channel_id = channel.channel_id;
            track.gm_channel1 = gm1;
            track.gm_channel2 = gm1 != kDefaultPercussionChannel ? gm2 : gm1;
            track.gm_program = channel.program;
        }

        Clef clef_from_tuning(const Song &song, const Track &track) {
            for (const Channel &channel : song.channels) {
                if (channel.channel_id == track.channel_id && channel.is_percussion()) {
                    return Clef::Treble;
                }
            }
            for (const int pitch : track.tuning_pitches) {
                if (pitch <= 34) {
                    return Clef::Bass;
                }
            }
            return Clef::Treble;
        }

        ParseResult<Ok> read_track(BinaryReader &stream, Song &song, Track &track, const int number,
                                   GtpContext &ctx, Lyric lyric) {
            if (auto flags = stream.read_u8(); !flags) {
                return fail_from<Ok>(flags);
            }
            if (ctx.format == Version::Version5 && (number == 1 || ctx.code == 0)) {
                if (auto step = stream.skip(1); !step) {
                    return step;
                }
            }
            track.number = number;
            track.lyrics = std::move(lyric);
            auto name = stream.read_string_byte(Constants::ShiftByte40);
            if (!name) {
                return fail_from<Ok>(name);
            }
            track.name = std::move(name.value());
            auto string_count = stream.read_i32();
            if (!string_count) {
                return fail_from<Ok>(string_count);
            }
            for (int i = 0; i < Constants::ShiftByte7; ++i) {
                auto tuning = stream.read_i32();
                if (!tuning) {
                    return fail_from<Ok>(tuning);
                }
                if (i < string_count.value()) {
                    track.tuning_pitches.push_back(tuning.value());
                }
            }
            if (auto port = stream.read_i32(); !port) {
                return fail_from<Ok>(port);
            }
            auto gm1 = stream.read_i32();
            auto gm2 = stream.read_i32();
            if (!gm1 || !gm2) {
                return ParseResult<Ok>::failure(
                    {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
            }
            apply_track_channel(song, track, gm1.value() - 1, gm2.value() - 1, ctx.mixer);
            if (auto frets = stream.read_i32(); !frets) {
                return fail_from<Ok>(frets);
            }
            auto capo = stream.read_i32();
            if (!capo) {
                return fail_from<Ok>(capo);
            }
            track.capo = capo.value();
            auto color = read_color(stream);
            if (!color) {
                return fail_from<Ok>(color);
            }
            track.color = color.value();
            if (ctx.format == Version::Version5) {
                if (auto step = stream.skip(5); !step) {
                    return step;
                }
                auto clef = stream.read_i32();
                if (!clef) {
                    return fail_from<Ok>(clef);
                }
                ctx.clefs[static_cast<std::size_t>(number - 1)] =
                    clef.value() == 12 ? Clef::Bass : Clef::Treble;
                const std::size_t extra = ctx.code > 0 ? 40 : 35;
                if (auto step = stream.skip(extra); !step) {
                    return step;
                }
                if (ctx.code > 0) {
                    if (auto a = stream.read_string_byte_size_of_integer(); !a) {
                        return fail_from<Ok>(a);
                    }
                    if (auto b = stream.read_string_byte_size_of_integer(); !b) {
                        return fail_from<Ok>(b);
                    }
                }
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_measure(BinaryReader &stream, Measure &measure, Track &track,
                                     GtpContext &ctx, const MeasureHeader &header) {
            const int voices = ctx.format == Version::Version5 ? kMaxVoices : 1;
            for (int voice = 0; voice < voices; ++voice) {
                long start = header.start;
                auto beats = stream.read_i32();
                if (!beats) {
                    return fail_from<Ok>(beats);
                }
                for (int i = 0; i < beats.value(); ++i) {
                    auto advance = read_beat(stream, measure, track, ctx, start, voice);
                    if (!advance) {
                        return fail_from<Ok>(advance);
                    }
                    start += advance.value();
                }
            }
            if (ctx.format == Version::Version5) {
                measure.beats.erase(std::remove_if(measure.beats.begin(), measure.beats.end(),
                                                   [](const Beat &beat) {
                                                       return beat.voice(0).empty && beat.voice(1).empty;
                                                   }),
                                    measure.beats.end());
                measure.clef = ctx.clefs[static_cast<std::size_t>(track.number - 1)];
            }
            const int index = header.number - 1;
            if (index >= 0 && static_cast<std::size_t>(index) < ctx.key_signatures.size()) {
                measure.key_signature = ctx.key_signatures[static_cast<std::size_t>(index)];
            }
            return ParseResult<Ok>::success(Ok{});
        }

    } // namespace

    void normalize_gtp_channels(Song &song) {
        for (Channel &channel : song.channels) {
            if (!channel.is_percussion()) {
                if (const ChannelParameter *param = find_parameter(channel, kGmChannel1Key)) {
                    if (param->value == "9") {
                        channel.bank = kDefaultPercussionBank;
                    }
                }
            }
            if (channel.is_percussion()) {
                channel.program = 0;
                channel.bank = kDefaultPercussionBank;
                set_parameter(channel, kGmChannel1Key, "9");
                set_parameter(channel, kGmChannel2Key, "9");
            }
        }
    }

    bool is_gtp_file(const std::span<const std::uint8_t> data) {
        if (data.size() < Constants::ShiftByte31) {
            return false;
        }
        const int len = (data[0] <= Constants::ShiftByte30) ? static_cast<int>(data[0])
                                                            : Constants::ShiftByte30;
        constexpr std::string_view prefix("FICHIER GUITAR PRO");
        if (std::cmp_less(len, prefix.size())) {
            return false;
        }
        return std::memcmp(data.subspan(1).data(), prefix.data(), prefix.size()) == 0;
    }

    ParseResult<Song> load_gtp_song(const std::vector<std::uint8_t> &data) {
        BinaryReader stream(data);
        auto version = stream.read_gtp_version();
        if (!version) {
            return fail_from<Song>(version);
        }
        auto detected = detect_version(version.value());
        if (!detected) {
            return fail_from<Song>(detected);
        }

        Song song;
        GtpContext ctx;
        ctx.format = detected.value().format;
        ctx.code = detected.value().code;

        ParseResult<Ok> info = ctx.format == Version::Version5
                                   ? read_info_gp5(stream, song.metadata)
                                   : read_info_gp3_gp4(stream, song.metadata);
        if (!info) {
            return fail_from<Song>(info);
        }

        Lyric lyric;
        int lyric_track = 0;
        if (ctx.format == Version::Version3 || ctx.format == Version::Version4) {
            auto feel = stream.read_bool();
            if (!feel) {
                return fail_from<Song>(feel);
            }
            ctx.triplet_feel = feel.value() ? TripletFeel::Eighth : TripletFeel::None;
        }
        if (ctx.format >= Version::Version4) {
            auto lyric_track_id = stream.read_i32();
            if (!lyric_track_id) {
                return fail_from<Song>(lyric_track_id);
            }
            lyric_track = lyric_track_id.value();
            auto lyrics = read_lyrics(stream);
            if (!lyrics) {
                return fail_from<Song>(lyrics);
            }
            lyric = std::move(lyrics.value());
        }
        if (ctx.format == Version::Version5) {
            if (auto step = stream.skip_page_setup(ctx.code); !step) {
                return fail_from<Song>(step);
            }
        }

        auto tempo = stream.read_i32();
        if (!tempo) {
            return fail_from<Song>(tempo);
        }
        constexpr int kMaxTempo{400};
        if (tempo.value() > 0 && tempo.value() <= kMaxTempo) {
            song.tempo_bpm = tempo.value();
            ctx.tempo.quarter_bpm = tempo.value();
        }
        if (ctx.format == Version::Version5 && ctx.code > 0) {
            if (auto step = stream.skip(1); !step) {
                return fail_from<Song>(step);
            }
        }

        auto key = read_key_signature(stream);
        if (!key) {
            return fail_from<Song>(key);
        }
        if (auto step = stream.skip(3); !step) {
            return fail_from<Song>(step);
        }
        if (ctx.format >= Version::Version4) {
            if (auto octave = stream.read_u8(); !octave) {
                return fail_from<Song>(octave);
            }
        }

        auto mixer = read_channels(stream);
        if (!mixer) {
            return fail_from<Song>(mixer);
        }
        ctx.mixer = std::move(mixer.value());

        if (ctx.format == Version::Version5) {
            if (auto step = stream.skip(Constants::ShiftByte42); !step) {
                return fail_from<Song>(step);
            }
        }

        auto measures = stream.read_i32();
        auto tracks = stream.read_i32();
        if (!measures || !tracks) {
            return ParseResult<Song>::failure(
                {.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
        }
        constexpr int kMaxTracks{1000};
        if (tracks.value() < 0 || tracks.value() > kMaxTracks) {
            return ParseResult<Song>::failure(
                {.code = ParseErrorCode::Unsupported, .message = "Invalid track count in GTP file"});
        }

        ctx.key_signatures.assign(static_cast<std::size_t>(std::max(measures.value(), 0)), key.value());
        ctx.clefs.assign(static_cast<std::size_t>(std::max(tracks.value(), 0)), Clef::Treble);

        TimeSignature time_signature;
        for (int i = 0; i < measures.value(); ++i) {
            if (ctx.format == Version::Version5 && i > 0) {
                if (auto step = stream.skip(1); !step) {
                    return fail_from<Song>(step);
                }
            }
            ParseResult<MeasureHeader> header =
                ctx.format == Version::Version5
                    ? read_measure_header_gp5(stream, i, time_signature, ctx)
                    : read_measure_header_gp34(stream, i + 1, song, time_signature, ctx);
            if (!header) {
                return fail_from<Song>(header);
            }
            song.measure_headers.push_back(std::move(header.value()));
            if (i + 1 < measures.value()) {
                ctx.key_signatures[static_cast<std::size_t>(i + 1)] =
                    ctx.key_signatures[static_cast<std::size_t>(i)];
            }
        }

        song.tracks.resize(static_cast<std::size_t>(tracks.value()));
        for (int i = 0; i < tracks.value(); ++i) {
            Lyric track_lyric = (i + 1 == lyric_track) ? lyric : Lyric{};
            if (auto step =
                    read_track(stream, song, song.tracks[static_cast<std::size_t>(i)], i + 1, ctx,
                               std::move(track_lyric));
                !step) {
                return fail_from<Song>(step);
            }
        }
        if (ctx.format == Version::Version5) {
            const std::size_t tail = ctx.code == 0 ? 2 : 1;
            if (auto step = stream.skip(tail); !step) {
                return fail_from<Song>(step);
            }
        }

        long start = kQuarterTime;
        for (int m = 0; m < measures.value(); ++m) {
            MeasureHeader &header = song.measure_headers[static_cast<std::size_t>(m)];
            header.start = start;
            for (int t = 0; t < tracks.value(); ++t) {
                Track &track = song.tracks[static_cast<std::size_t>(t)];
                Measure measure;
                measure.header_index = m;
                if (ctx.format != Version::Version5) {
                    measure.clef = clef_from_tuning(song, track);
                }
                if (auto step = read_measure(stream, measure, track, ctx, header); !step) {
                    return fail_from<Song>(step);
                }
                track.measures.push_back(std::move(measure));
                // Some GP5.10 writers omit the final measure×track padding byte; TuxGuitar's
                // skip() silently accepts a short EOF read. Tolerate EOF here only.
                if (ctx.format == Version::Version5 && !stream.eof()) {
                    if (auto step = stream.skip(1); !step) {
                        return fail_from<Song>(step);
                    }
                }
            }
            header.tempo = ctx.tempo;
            start += header.length();
        }

        if (!song.measure_headers.empty()) {
            song.tempo_bpm = song.measure_headers.front().tempo.quarter_bpm;
        }
        normalize_gtp_channels(song);
        return ParseResult<Song>::success(std::move(song));
    }

} // namespace libgp_parser
