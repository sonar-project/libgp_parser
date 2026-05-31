#include <iostream>
#include <span>
#include <vector>

#include <libgp_parser/load_song.hpp>

int main(int argc, char *argv[]) {
    try {
        std::span<char *> args(argv, argc);
        if (args.size() < 2) {
            std::cerr << "Usage: " << (!args.empty() ? args[0] : "gp_info") << " <file.gpx>\n";
            return 1;
        }

        const auto result = libgp_parser::load_song(args[1]);
        if (!result) {
            std::cerr << "Error: " << result.error().message << '\n';
            return 1;
        }

        const libgp_parser::Song &song = result.value();
        std::cout << "Title:  " << song.name() << '\n';
        std::cout << "Artist: " << song.artist() << '\n';
        std::cout << "Album:  " << song.album() << '\n';
        std::cout << "Author: " << song.author() << '\n';
        if (song.has_tempo()) {
            std::cout << "Tempo:  " << song.tempo_bpm << " BPM\n";
        } else {
            std::cout << "Tempo:  not found\n";
        }
        std::cout << "Tracks: " << song.track_count() << '\n';

        // --- Read tuning ---
        for (const libgp_parser::Track &track : song.tracks) {
            std::cout << "  [" << track.number << "] " << track.name << '\n';

            if (!track.tuning_pitches.empty()) {
                std::cout << "       Tuning (Noten):  " << track.tuning_display << '\n';
                std::cout << "       Tuning (MIDI):   ";
                for (int pitch : track.tuning_pitches) {
                    std::cout << pitch << " ";
                }
                std::cout << '\n';
            } else {
                std::cout << "       Tuning:          No data (possibly standard tuning "
                             "or pure MIDI instrument)\n";
            }

            if (track.capo > 0) {
                std::cout << "       Capo:            Fret " << track.capo << '\n';
            }
        }

    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "An unknown fatal error occurred.\n";
        return 1;
    }
    return 0;
}