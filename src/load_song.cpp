#include "libgp_parser/load_song.hpp"

#include "libgp_parser/byte_string.hpp"
#include "libgp_parser/gpx_format.hpp"
#include "libgp_parser/gpx_score_bundle.hpp"
#include "libgp_parser/gpx_score_reader.hpp"
#include "libgp_parser/gpx_tempo_reader.hpp"
#include "libgp_parser/gpx_track_reader.hpp"
#include "libgp_parser/gtp_reader.hpp"
#include "libgp_parser/tuning_pitches.hpp"

#include <fstream>
#include <iterator>

namespace libgp_parser {
    namespace {

        ParseResult<std::vector<std::uint8_t>> read_file_bytes(const std::filesystem::path &path) {
            std::ifstream inputStream(path, std::ios::binary);
            if (!inputStream) {
                return ParseResult<std::vector<std::uint8_t>>::failure(
                    {.code = ParseErrorCode::Io, .message = "Cannot open file: " + path.string()});
            }
            return ParseResult<std::vector<std::uint8_t>>::success(std::vector<std::uint8_t>(
                (std::istreambuf_iterator<char>(inputStream)), std::istreambuf_iterator<char>()));
        }

        ParseResult<Song> load_gpx_song(const std::vector<std::uint8_t> &file_data) {
            auto gpif = extract_score_gpif(file_data);
            if (!gpif) {
                return ParseResult<Song>::failure(gpif.error());
            }

            const std::string xml = bytes_to_string(gpif.value());
            const std::string_view xml_view{xml};
            const bool is_gp7 = is_zip_header(file_data);

            auto metadata = parse_gpx_score_metadata(xml_view);
            if (!metadata) {
                return ParseResult<Song>::failure(metadata.error());
            }

            auto tracks = parse_gpx_tracks(xml_view, is_gp7);
            if (!tracks) {
                return ParseResult<Song>::failure(tracks.error());
            }

            auto tempo = parse_gpx_initial_tempo(xml_view);
            if (!tempo) {
                return ParseResult<Song>::failure(tempo.error());
            }

            Song song;
            song.metadata = std::move(metadata.value());
            song.tracks = std::move(tracks.value());
            song.tempo_bpm = tempo.value();
            return ParseResult<Song>::success(std::move(song));
        }

    } // namespace

    ParseResult<Song> load_song(const std::filesystem::path &path) {
        auto file = read_file_bytes(path);
        if (!file) {
            return ParseResult<Song>::failure(file.error());
        }

        const auto &bytes = file.value();
        if (bytes.size() < 4) {
            return ParseResult<Song>::failure(
                {.code = ParseErrorCode::Unsupported, .message = "File too short"});
        }

        auto result = is_gtp_file(bytes) ? load_gtp_song(bytes) : load_gpx_song(bytes);
        if (!result.has_value()) {
            return result;
        }

        Song &song = result.value();
        for (Track &track : song.tracks) {
            finalize_track_tuning(track);
        }

        return result;
    }

} // namespace libgp_parser
