#include "libgp_parser/gtp_reader.hpp"
#include "libgp_parser/binary_reader.hpp"
#include "libgp_parser/bit_constants.hpp"
#include "libgp_parser/flag_constants.hpp"
#include "libgp_parser/version_constants.hpp"

#include <cstring>

namespace libgp_parser {
    namespace {

        struct GtpVersion {
            int format; // 3, 4, or 5
            int code;
        };

        ParseResult<GtpVersion> detect_version(const std::string &version) {
            if (version == "FICHIER GUITAR PRO v3.00") {
                return ParseResult<GtpVersion>::success({Version::Version3, 0});
            }
            if (version == "FICHIER GUITAR PRO v4.00") {
                return ParseResult<GtpVersion>::success({Version::Version4, 0});
            }
            if (version == "FICHIER GUITAR PRO v4.06") {
                return ParseResult<GtpVersion>::success({Version::Version4, 1});
            }
            if (version == "FICHIER GUITAR PRO L4.06") {
                return ParseResult<GtpVersion>::success({Version::Version4, 2});
            }
            if (version == "FICHIER GUITAR PRO v5.00") {
                return ParseResult<GtpVersion>::success({Version::Version5, 0});
            }
            if (version == "FICHIER GUITAR PRO v5.10") {
                return ParseResult<GtpVersion>::success({Version::Version5, 1});
            }
            return ParseResult<GtpVersion>::failure(
                {ParseErrorCode::Unsupported, "Unsupported GTP version: " + version});
        }

        ParseResult<Ok> read_info_comments(BinaryReader &inputStream, SongMetadata &metadata);

        ParseResult<Ok> read_string_field(BinaryReader &inputStream, std::string &out) {
            auto value = inputStream.read_string_byte_size_of_integer();
            if (!value.has_value()) {
                return ParseResult<Ok>::failure(value.error());
            }
            out = std::move(value.value());
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> skip_string_field(BinaryReader &inputStream) {
            auto value = inputStream.read_string_byte_size_of_integer();
            if (!value.has_value()) {
                return ParseResult<Ok>::failure(value.error());
            }
            return ParseResult<Ok>::success(Ok{});
        }

        /// GP3/GP4 readInfo — no extra field between author and copyright.
        ParseResult<Ok> read_info_gp3_gp4(BinaryReader &inputStream, SongMetadata &metadata) {
            if (auto result = read_string_field(inputStream, metadata.title); !result.has_value()) {
                return result;
            }
            if (auto result = skip_string_field(inputStream); !result.has_value()) {
                return result;
            }
            if (auto result = read_string_field(inputStream, metadata.artist);
                !result.has_value()) {
                return result;
            }
            if (auto result = read_string_field(inputStream, metadata.album); !result.has_value()) {
                return result;
            }
            if (auto result = read_string_field(inputStream, metadata.words_and_music);
                !result.has_value()) {
                return result;
            }
            if (auto result = read_string_field(inputStream, metadata.copyright);
                !result.has_value()) {
                return result;
            }
            if (auto result = read_string_field(inputStream, metadata.tabber);
                !result.has_value()) {
                return result;
            }
            if (auto result = skip_string_field(inputStream); !result.has_value()) {
                return result;
            }
            return read_info_comments(inputStream, metadata);
        }

        /// GP5 readInfo — skip field between author and copyright.
        ParseResult<Ok> read_info_gp5(BinaryReader &inputStream, SongMetadata &metadata) {
            if (auto result = read_string_field(inputStream, metadata.title); !result.has_value()) {
                return result;
            }
            if (auto result = skip_string_field(inputStream); !result.has_value()) {
                return result;
            }
            if (auto result = read_string_field(inputStream, metadata.artist);
                !result.has_value()) {
                return result;
            }
            if (auto result = read_string_field(inputStream, metadata.album); !result.has_value()) {
                return result;
            }
            if (auto result = read_string_field(inputStream, metadata.words_and_music);
                !result.has_value()) {
                return result;
            }
            if (auto result = skip_string_field(inputStream); !result.has_value()) {
                return result;
            }
            if (auto result = read_string_field(inputStream, metadata.copyright);
                !result.has_value()) {
                return result;
            }
            if (auto result = read_string_field(inputStream, metadata.tabber);
                !result.has_value()) {
                return result;
            }
            if (auto result = skip_string_field(inputStream); !result.has_value()) {
                return result;
            }
            return read_info_comments(inputStream, metadata);
        }

        ParseResult<Ok> read_info_comments(BinaryReader &inputStream, SongMetadata &metadata) {
            auto comments_count = inputStream.read_i32();
            if (!comments_count.has_value()) {
                return ParseResult<Ok>::failure(comments_count.error());
            }
            metadata.notices.clear();
            for (int i = 0; i < comments_count.value(); ++i) {
                auto comment = inputStream.read_string_byte_size_of_integer();
                if (!comment.has_value()) {
                    return ParseResult<Ok>::failure(comment.error());
                }
                metadata.notices += comment.value();
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<std::int32_t> read_key_signature(BinaryReader &inputStream) {
            auto byte = inputStream.read_u8();
            if (!byte.has_value()) {
                return ParseResult<std::int32_t>::failure(byte.error());
            }
            // Avoid converting signed char to int directly; cast to unsigned char first
            int key = static_cast<int>(static_cast<unsigned char>(byte.value()));
            if (key < 0) {
                key = Constants::ShiftByte7 - key;
            }
            return ParseResult<std::int32_t>::success(key);
        }

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

        ParseResult<Ok> skip_if_flag(const int flags, const int mask, BinaryReader &stream,
                                     const std::size_t bytes) {
            if (!flag_set(flags, mask)) {
                return ParseResult<Ok>::success(Ok{});
            }
            return stream.skip(bytes);
        }

        ParseResult<Ok> read_u8_if_flag(const int mask, BinaryReader &stream, const int flags) {
            if (!flag_set(flags, mask)) {
                return ParseResult<Ok>::success(Ok{});
            }
            return require(stream.read_u8());
        }

        ParseResult<Ok> skip_key_signature_ex(BinaryReader &stream, const int flags) {
            if (!flag_set(flags, FileFlag::HAS_KEY_SIGNATURE_EX)) {
                return ParseResult<Ok>::success(Ok{});
            }
            if (auto ksignature = read_key_signature(stream); !ksignature.has_value()) {
                return ParseResult<Ok>::failure(ksignature.error());
            }
            return stream.skip(1);
        }

        ParseResult<Ok> skip_gp5_tonality_tail(BinaryReader &stream, const int flags) {
            if (flag_set(flags, FileFlag::HAS_TONALITY)) {
                return require(stream.read_u8());
            }
            return stream.skip(1);
        }

        ParseResult<Ok> skip_color(BinaryReader &inputStream) {
            if (auto result = inputStream.skip(3); !result.has_value()) {
                return result;
            }
            return inputStream.skip(1);
        }

        ParseResult<Ok> skip_marker(BinaryReader &inputStream) {
            if (auto title = inputStream.read_string_byte_size_of_integer(); !title.has_value()) {
                return ParseResult<Ok>::failure(title.error());
            }
            return skip_color(inputStream);
        }

        ParseResult<Ok> skip_measure_header_gp34(BinaryReader &inputStream) {
            auto flags = inputStream.read_u8();
            if (!flags.has_value()) {
                return ParseResult<Ok>::failure(flags.error());
            }
            const int flag = static_cast<int>(flags.value());
            // ParseResult<Ok> skip_if_flag(const int flags, const int mask, BinaryReader &stream,
            // const std::size_t bytes)
            if (auto step = skip_if_flag(flag, FileFlag::HAS_NUMBER_OF_TUPLETS, inputStream, 1);
                !step) {
                return step;
            }
            if (auto step = skip_if_flag(flag, FileFlag::HAS_MEASURE_DURATION, inputStream, 1);
                !step) {
                return step;
            }
            if (auto step = skip_if_flag(flag, FileFlag::HAS_MARKER, inputStream, 1); !step) {
                return step;
            }
            if (auto step = read_u8_if_flag(FileFlag::HAS_TONALITY, inputStream, flag); !step) {
                return step;
            }
            if (flag_set(flag, FileFlag::HAS_DOUBLE_BARLINE)) {
                if (auto step = skip_marker(inputStream); !step) {
                    return step;
                }
            }
            return skip_key_signature_ex(inputStream, flag);
        }

        ParseResult<Ok> skip_measure_header_gp5(BinaryReader &inputStream) {
            auto flags = inputStream.read_u8();
            if (!flags.has_value()) {
                return ParseResult<Ok>::failure(flags.error());
            }
            const int flag = static_cast<int>(flags.value());
            if (auto step = skip_if_flag(flag, FileFlag::HAS_NUMBER_OF_TUPLETS, inputStream, 1);
                !step) {
                return step;
            }
            if (auto step = skip_if_flag(flag, FileFlag::HAS_MEASURE_DURATION, inputStream, 1);
                !step) {
                return step;
            }
            if (auto step = read_u8_if_flag(FileFlag::HAS_MARKER, inputStream, flag); !step) {
                return step;
            }
            if (flag_set(flag, FileFlag::HAS_DOUBLE_BARLINE)) {
                if (auto step = skip_marker(inputStream); !step) {
                    return step;
                }
            }
            if (auto step = skip_key_signature_ex(inputStream, flag); !step) {
                return step;
            }
            if (flag_set(flag, FileFlag::HAS_NUMBER_OF_TUPLETS) ||
                flag_set(flag, FileFlag::HAS_MEASURE_DURATION)) {
                if (auto step = inputStream.skip(4); !step) {
                    return step;
                }
            }
            if (auto step = skip_gp5_tonality_tail(inputStream, flag); !step) {
                return step;
            }
            return require(inputStream.read_u8());
        }

        ParseResult<Ok> skip_measure_headers(int fmt, BinaryReader &inputStream,
                                             int measure_count) {
            for (int i = 0; i < measure_count; ++i) {
                if (fmt == Constants::ShiftByte5 && i > 0) {
                    if (auto result = inputStream.skip(1); !result.has_value()) {
                        return result;
                    }
                }
                ParseResult<Ok> skipped = (fmt == Constants::ShiftByte5)
                                              ? skip_measure_header_gp5(inputStream)
                                              : skip_measure_header_gp34(inputStream);
                if (!skipped.has_value()) {
                    return skipped;
                }
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> skip_gp5_track_lead_in(BinaryReader &inputStream, const int number,
                                               const int fmt, const int code) {
            const bool needs_lead_in = fmt == Constants::ShiftByte5 && (number == 1 || code == 0);
            if (!needs_lead_in) {
                return ParseResult<Ok>::success(Ok{});
            }
            return inputStream.skip(1);
        }

        ParseResult<Ok> read_gtp_track_tuning(BinaryReader &inputStream, Track &track) {
            auto string_count = inputStream.read_i32();
            if (!string_count.has_value()) {
                return ParseResult<Ok>::failure(string_count.error());
            }
            if (string_count.value() < 0 || string_count.value() > Constants::ShiftByte7) {
                return ParseResult<Ok>::failure({.code = ParseErrorCode::Unsupported,
                                                 .message = "Invalid string count in GTP track"});
            }

            track.tuning_pitches.clear();
            for (int i = 0; i < Constants::ShiftByte7; ++i) {
                auto tuning = inputStream.read_i32();
                if (!tuning.has_value()) {
                    return ParseResult<Ok>::failure(tuning.error());
                }
                if (i < string_count.value()) {
                    track.tuning_pitches.push_back(tuning.value());
                }
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_gtp_track_midi(BinaryReader &inputStream, Track &track) {
            if (auto step = require(inputStream.read_i32()); !step) {
                return step;
            }

            auto gm1 = inputStream.read_i32();
            auto gm2 = inputStream.read_i32();
            if (!gm1.has_value()) {
                return ParseResult<Ok>::failure(gm1.error());
            }
            if (!gm2.has_value()) {
                return ParseResult<Ok>::failure(gm2.error());
            }
            track.gm_channel1 = gm1.value() - 1;
            track.gm_channel2 = gm2.value() - 1;

            return require(inputStream.read_i32());
        }

        ParseResult<Ok> read_gtp_track_color(BinaryReader &inputStream, Track &track) {
            auto red = inputStream.read_u8();
            auto green = inputStream.read_u8();
            auto blue = inputStream.read_u8();
            if (!red.has_value() || !green.has_value() || !blue.has_value()) {
                return ParseResult<Ok>::failure(
                    ParseError{.code = ParseErrorCode::Io, .message = "Unexpected end of file"});
            }
            track.color = RgbColor{
                .r = static_cast<std::uint8_t>(red.value()),
                .g = static_cast<std::uint8_t>(green.value()),
                .b = static_cast<std::uint8_t>(blue.value()),
            };
            return inputStream.skip(1);
        }

        ParseResult<Ok> skip_gtp_track_gp5_tail(BinaryReader &inputStream, const int code) {
            const std::size_t extra = (code > 0) ? 49 : 44;
            if (auto step = inputStream.skip(extra); !step) {
                return step;
            }
            if (code <= 0) {
                return ParseResult<Ok>::success(Ok{});
            }
            if (auto step = require(inputStream.read_string_byte_size_of_integer()); !step) {
                return step;
            }
            return require(inputStream.read_string_byte_size_of_integer());
        }

        ParseResult<Ok> read_gtp_track(BinaryReader &inputStream, Track &track, int number, int fmt,
                                       int code) {
            if (auto step = require(inputStream.read_u8()); !step) {
                return step;
            }
            if (auto step = skip_gp5_track_lead_in(inputStream, number, fmt, code); !step) {
                return step;
            }

            auto name = inputStream.read_string_byte(Constants::ShiftByte40);
            if (!name.has_value()) {
                return ParseResult<Ok>::failure(name.error());
            }
            track.name = std::move(name.value());

            if (auto step = read_gtp_track_tuning(inputStream, track); !step) {
                return step;
            }
            if (auto step = read_gtp_track_midi(inputStream, track); !step) {
                return step;
            }

            auto capo = inputStream.read_i32();
            if (!capo.has_value()) {
                return ParseResult<Ok>::failure(capo.error());
            }
            track.capo = capo.value();

            if (auto step = read_gtp_track_color(inputStream, track); !step) {
                return step;
            }

            if (fmt == Constants::ShiftByte5) {
                return skip_gtp_track_gp5_tail(inputStream, code);
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_gtp_tracks(BinaryReader &inputStream, Song &song, int fmt, int code) {
            for (Track &track : song.tracks) {
                if (auto result = read_gtp_track(inputStream, track, track.number, fmt, code);
                    !result.has_value()) {
                    return result;
                }
            }

            if (fmt == Constants::ShiftByte5) {
                const std::size_t tail = (code == 0) ? 2 : 1;
                if (auto result = inputStream.skip(tail); !result.has_value()) {
                    return result;
                }
            }

            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<GtpVersion> read_gtp_file_version(BinaryReader &inputStream) {
            auto version_str = inputStream.read_gtp_version();
            if (!version_str.has_value()) {
                return fail_from<GtpVersion>(version_str);
            }
            return detect_version(version_str.value());
        }

        ParseResult<Ok> read_gtp_song_metadata(BinaryReader &inputStream, SongMetadata &metadata,
                                               const int fmt) {
            if (fmt == Constants::ShiftByte5) {
                return read_info_gp5(inputStream, metadata);
            }
            return read_info_gp3_gp4(inputStream, metadata);
        }

        ParseResult<Ok> skip_gtp_format_prefix(BinaryReader &inputStream,
                                               const GtpVersion &version) {
            const int fmt = version.format;
            const int code = version.code;
            if (fmt == Version::Version3 || fmt == Version::Version4) {
                if (auto step = require(inputStream.read_bool()); !step) {
                    return step;
                }
            }

            if (fmt >= Version::Version4) {
                if (auto step = inputStream.skip(4); !step) {
                    return step;
                }
                if (auto step = inputStream.skip_lyrics(); !step) {
                    return step;
                }
            }

            if (fmt == Version::Version5) {
                return inputStream.skip_page_setup(code);
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_gtp_song_tempo(BinaryReader &inputStream, Song &song, const int fmt,
                                            const int code) {
            auto tempo = inputStream.read_i32();
            if (!tempo.has_value()) {
                return ParseResult<Ok>::failure(
                    {.code = ParseErrorCode::Io, .message = "Failed to read tempo"});
            }
            constexpr int userMaxTempo{400};
            if (tempo.value() > 0 && tempo.value() <= userMaxTempo) {
                song.tempo_bpm = tempo.value();
            }

            if (fmt == Version::Version5 && code > 0) {
                return inputStream.skip(1);
            }
            return ParseResult<Ok>::success(Ok{});
        }

        ParseResult<Ok> read_gtp_song_key_and_channels(BinaryReader &inputStream, const int fmt) {
            if (auto ksignature = read_key_signature(inputStream); !ksignature.has_value()) {
                return ParseResult<Ok>::failure(ksignature.error());
            }
            if (auto step = inputStream.skip(3); !step) {
                return step;
            }

            if (fmt >= Version::Version4) {
                if (auto step = require(inputStream.read_u8()); !step) {
                    return step;
                }
            }

            return inputStream.skip_channels();
        }

        ParseResult<Ok> read_gtp_song_layout(BinaryReader &inputStream, Song &song, const int fmt,
                                             const int code) {
            if (fmt == Version::Version5) {
                if (auto step = inputStream.skip(Constants::ShiftByte42); !step) {
                    return step;
                }
            }

            auto measures = inputStream.read_i32();
            auto tracks = inputStream.read_i32();
            if (!measures.has_value()) {
                return ParseResult<Ok>::failure(measures.error());
            }
            if (!tracks.has_value()) {
                return ParseResult<Ok>::failure(tracks.error());
            }

            constexpr int outOfTrack{1000};
            if (tracks.value() < 0 || tracks.value() > outOfTrack) {
                return ParseResult<Ok>::failure({.code = ParseErrorCode::Unsupported,
                                                 .message = "Invalid track count in GTP file"});
            }

            song.tracks.resize(static_cast<std::size_t>(tracks.value()));
            for (int i = 0; i < tracks.value(); ++i) {
                song.tracks[static_cast<std::size_t>(i)].number = i + 1;
            }

            if (auto step = skip_measure_headers(fmt, inputStream, measures.value()); !step) {
                return step;
            }
            return read_gtp_tracks(inputStream, song, fmt, code);
        }

    } // namespace

    bool is_gtp_file(const std::span<const std::uint8_t> data) {
        if (data.size() < Constants::ShiftByte31) {
            return false;
        }
        const int len = (data[0] <= Constants::ShiftByte30) ? static_cast<int>(data[0])
                                                            : Constants::ShiftByte30;
        constexpr std::string_view prefix("FICHIER GUITAR PRO");
        if (len < static_cast<int>(prefix.size())) {
            return false;
        }
        return std::memcmp(data.subspan(1).data(), prefix.data(), prefix.size()) == 0;
    }

    ParseResult<Song> load_gtp_song(const std::vector<std::uint8_t> &data) {
        BinaryReader inputStream(data);

        auto version = read_gtp_file_version(inputStream);
        if (!version.has_value()) {
            return fail_from<Song>(version);
        }

        Song song;
        const int fmt = version.value().format;
        const int code = version.value().code;

        if (auto step = read_gtp_song_metadata(inputStream, song.metadata, fmt); !step) {
            return fail_from<Song>(step);
        }
        if (auto step = skip_gtp_format_prefix(inputStream, version.value()); !step) {
            return fail_from<Song>(step);
        }
        if (auto step = read_gtp_song_tempo(inputStream, song, fmt, code); !step) {
            return fail_from<Song>(step);
        }
        if (auto step = read_gtp_song_key_and_channels(inputStream, fmt); !step) {
            return fail_from<Song>(step);
        }
        if (auto step = read_gtp_song_layout(inputStream, song, fmt, code); !step) {
            return fail_from<Song>(step);
        }

        return ParseResult<Song>::success(std::move(song));
    }

} // namespace libgp_parser
