#pragma once

#include "libgp_parser/channel.hpp"
#include "libgp_parser/measure.hpp"
#include "libgp_parser/track.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace libgp_parser {

    /// Metadata from the GPX Score element (intermediate GPXScore model).
    /// @par TuxGuitar source
    /// common/TuxGuitar-gpx/.../score/GPXScore.java; mapped to TGSong in
    /// GPXDocumentParser.parseScore() (title→name, tabber→writer, …).
    /// @par Brief
    /// Title, artist, album, and other header fields.
    /// @par Visibility
    /// public (data type).
    struct SongMetadata {
        std::string title;
        std::string subtitle;
        std::string artist;
        std::string album;
        std::string words;
        std::string music;
        std::string words_and_music;
        std::string copyright;
        std::string tabber;
        std::string instructions;
        std::string notices;

        [[nodiscard]] bool operator==(const SongMetadata &) const = default;
    };

    /// Loaded Guitar Pro score without UI dependencies.
    /// @par TuxGuitar source
    /// Simplified TGSong / GPXDocument before GPXDocumentParser.parse().
    /// @par Brief
    /// Metadata, track list, and initial tempo.
    /// @par Visibility
    /// public (data type).
    struct Song {
        SongMetadata metadata;
        std::vector<Track> tracks;
        std::vector<MeasureHeader> measure_headers;
        std::vector<Channel> channels;

        /// Initial tempo in BPM (quarter notes per minute). 0 if not read.
        int tempo_bpm{0};

        // --- Convenience accessors aligned with TGSong naming (read-only) ---

        [[nodiscard]] const std::string &name() const noexcept { return metadata.title; }
        [[nodiscard]] const std::string &artist() const noexcept { return metadata.artist; }
        [[nodiscard]] const std::string &album() const noexcept { return metadata.album; }
        [[nodiscard]] const std::string &author() const noexcept {
            return metadata.words_and_music;
        }
        [[nodiscard]] const std::string &copyright() const noexcept { return metadata.copyright; }
        [[nodiscard]] const std::string &writer() const noexcept { return metadata.tabber; }
        [[nodiscard]] const std::string &comments() const noexcept { return metadata.notices; }

        [[nodiscard]] std::size_t track_count() const noexcept { return tracks.size(); }
        [[nodiscard]] std::size_t measure_count() const noexcept { return measure_headers.size(); }
        [[nodiscard]] bool has_tempo() const noexcept { return tempo_bpm > 0; }

        [[nodiscard]] const MeasureHeader &header_for(const Measure &measure) const {
            return measure_headers.at(static_cast<std::size_t>(measure.header_index));
        }

        [[nodiscard]] bool operator==(const Song &) const = default;
    };

} // namespace libgp_parser
