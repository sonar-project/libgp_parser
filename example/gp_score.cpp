#include <iostream>
#include <span>
#include <string>

#include <libgp_parser/load_song.hpp>

namespace {

    using libgp_parser::Beat;
    using libgp_parser::DurationValue;
    using libgp_parser::Measure;
    using libgp_parser::MeasureHeader;
    using libgp_parser::Note;
    using libgp_parser::NoteEffect;
    using libgp_parser::Song;
    using libgp_parser::StrokeDirection;
    using libgp_parser::Track;
    using libgp_parser::Voice;

    const char *duration_name(const int value) {
        switch (value) {
        case DurationValue::Whole:
            return "1";
        case DurationValue::Half:
            return "1/2";
        case DurationValue::Quarter:
            return "1/4";
        case DurationValue::Eighth:
            return "1/8";
        case DurationValue::Sixteenth:
            return "1/16";
        case DurationValue::ThirtySecond:
            return "1/32";
        case DurationValue::SixtyFourth:
            return "1/64";
        default:
            return "?";
        }
    }

    const char *stroke_name(const StrokeDirection direction) {
        switch (direction) {
        case StrokeDirection::Up:
            return "up";
        case StrokeDirection::Down:
            return "down";
        default:
            return "none";
        }
    }

    void print_effects(const NoteEffect &effect) {
        if (!effect.has_any()) {
            return;
        }
        std::cout << " effects=[";
        bool first = true;
        const auto add = [&](const char *name) {
            if (!first) {
                std::cout << ',';
            }
            first = false;
            std::cout << name;
        };
        if (effect.has_bend()) {
            add("bend");
        }
        if (effect.tremolo_bar) {
            add("tremolo-bar");
        }
        if (effect.harmonic) {
            add("harmonic");
        }
        if (effect.grace) {
            add("grace");
        }
        if (effect.trill) {
            add("trill");
        }
        if (effect.tremolo_picking) {
            add("tremolo-picking");
        }
        if (effect.vibrato) {
            add("vibrato");
        }
        if (effect.dead_note) {
            add("dead");
        }
        if (effect.slide) {
            add("slide");
        }
        if (effect.hammer) {
            add("hammer");
        }
        if (effect.ghost_note) {
            add("ghost");
        }
        if (effect.accentuated_note) {
            add("accent");
        }
        if (effect.heavy_accentuated_note) {
            add("heavy-accent");
        }
        if (effect.palm_mute) {
            add("palm-mute");
        }
        if (effect.staccato) {
            add("staccato");
        }
        if (effect.tapping) {
            add("tap");
        }
        if (effect.slapping) {
            add("slap");
        }
        if (effect.popping) {
            add("pop");
        }
        if (effect.fade_in) {
            add("fade-in");
        }
        if (effect.let_ring) {
            add("let-ring");
        }
        std::cout << ']';
    }

    void print_voice(const Voice &voice, const int index) {
        if (voice.empty && voice.notes.empty()) {
            return;
        }
        std::cout << "        voice " << index << ": "
                  << (voice.is_rest() ? "rest" : "notes") << " dur="
                  << duration_name(voice.duration.value);
        if (voice.duration.dotted) {
            std::cout << '.';
        }
        if (voice.duration.division.enters != 1 || voice.duration.division.times != 1) {
            std::cout << " tuplet=" << voice.duration.division.enters << ':'
                      << voice.duration.division.times;
        }
        std::cout << '\n';
        for (const Note &note : voice.notes) {
            std::cout << "          string=" << note.string << " fret=" << note.value
                      << " vel=" << note.velocity;
            if (note.tied_note) {
                std::cout << " tied";
            }
            print_effects(note.effect);
            std::cout << '\n';
        }
    }

    void print_beat(const Beat &beat) {
        std::cout << "      beat start=" << beat.start
                  << " stroke=" << stroke_name(beat.stroke.direction);
        if (beat.chord) {
            std::cout << " chord=\"" << beat.chord->name << '"';
        }
        if (!beat.text.empty()) {
            std::cout << " text=\"" << beat.text << '"';
        }
        std::cout << '\n';
        for (int v = 0; v < libgp_parser::kMaxVoices; ++v) {
            print_voice(beat.voice(v), v);
        }
    }

    void print_header(const MeasureHeader &header) {
        std::cout << "  measure #" << header.number << " start=" << header.start << " "
                  << header.time_signature.numerator << '/'
                  << header.time_signature.denominator.value
                  << " tempo=" << header.tempo.quarter_bpm;
        if (header.repeat_open) {
            std::cout << " |:";
        }
        if (header.repeat_close > 0) {
            std::cout << " :|x" << header.repeat_close;
        }
        if (header.repeat_alternative != 0) {
            std::cout << " alt=" << header.repeat_alternative;
        }
        if (header.has_marker()) {
            std::cout << " marker=\"" << header.marker->title << '"';
        }
        std::cout << '\n';
    }

    void print_song(const Song &song) {
        std::cout << "Title:     " << song.name() << '\n';
        std::cout << "Artist:    " << song.artist() << '\n';
        std::cout << "Album:     " << song.album() << '\n';
        std::cout << "Author:    " << song.author() << '\n';
        std::cout << "Tempo:     " << song.tempo_bpm << " BPM\n";
        std::cout << "Measures:  " << song.measure_count() << '\n';
        std::cout << "Tracks:    " << song.track_count() << '\n';
        std::cout << "Channels:  " << song.channels.size() << '\n';

        for (const auto &channel : song.channels) {
            std::cout << "  channel id=" << channel.channel_id << " program=" << channel.program
                      << " bank=" << channel.bank
                      << (channel.is_percussion() ? " (percussion)" : "") << '\n';
        }

        for (const MeasureHeader &header : song.measure_headers) {
            print_header(header);
        }

        for (const Track &track : song.tracks) {
            std::cout << "\nTrack [" << track.number << "] " << track.name
                      << " gm=" << track.gm_program << " channel_id=" << track.channel_id
                      << '\n';
            if (!track.tuning_display.empty()) {
                std::cout << "  tuning: " << track.tuning_display << '\n';
            }
            if (track.capo > 0) {
                std::cout << "  capo:   " << track.capo << '\n';
            }
            if (!track.lyrics.empty()) {
                std::cout << "  lyrics from measure " << track.lyrics.from << ": "
                          << track.lyrics.text << '\n';
            }

            for (const Measure &measure : track.measures) {
                const MeasureHeader &header = song.header_for(measure);
                std::cout << "  measure #" << header.number << " beats=" << measure.beats.size()
                          << '\n';
                for (const Beat &beat : measure.beats) {
                    print_beat(beat);
                }
            }
        }
    }

} // namespace

int main(int argc, char *argv[]) {
    try {
        const std::span<char *> args(argv, argc);
        if (args.size() < 2) {
            std::cerr << "Usage: " << (!args.empty() ? args[0] : "gp_score")
                      << " <file.gp|gp3|gp4|gp5|gpx>\n";
            return 1;
        }

        const auto result = libgp_parser::load_song(args[1]);
        if (!result) {
            std::cerr << "Error: " << result.error().message << '\n';
            return 1;
        }

        print_song(result.value());
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "An unknown fatal error occurred.\n";
        return 1;
    }
    return 0;
}
