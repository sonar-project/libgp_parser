#include "libgp_parser/timeline.hpp"

#include <algorithm>

namespace libgp_parser {
    namespace {

        /// Port of TuxGuitar MidiRepeatController (process / shouldPlay / getRepeatMove).
        class RepeatController {
          public:
            RepeatController(const Song &song, const int s_header, const int e_header)
                : song_(song), count_(static_cast<int>(song.measure_headers.size())),
                  s_header_(s_header), e_header_(e_header) {}

            void process() {
                const MeasureHeader &header = song_.measure_headers[static_cast<std::size_t>(index_)];

                if (e_header_ >= 0 && header.number > e_header_) {
                    should_play_ = false;
                    ++index_;
                    return;
                }

                // First measure of the song (or of a practice loop) is an implicit repeat open.
                if ((s_header_ >= 0 && header.number == s_header_) || header.number == 1) {
                    repeat_start_index_ = index_;
                    repeat_start_ = header.start;
                    repeat_open_ = true;
                }

                should_play_ = true;

                if (header.repeat_open) {
                    repeat_start_index_ = index_;
                    repeat_start_ = header.start;
                    repeat_open_ = true;
                    if (index_ > last_index_) {
                        repeat_number_ = 0;
                        repeat_alternative_ = 0;
                    }
                } else {
                    if (repeat_alternative_ == 0) {
                        repeat_alternative_ = header.repeat_alternative;
                    }
                    if (repeat_open_ && repeat_alternative_ > 0 &&
                        (repeat_alternative_ & (1 << repeat_number_)) == 0) {
                        repeat_move_ -= header.length();
                        if (header.repeat_close > 0) {
                            repeat_alternative_ = 0;
                        }
                        should_play_ = false;
                    }
                }

                if (should_play_) {
                    last_index_ = std::max(last_index_, index_);

                    const bool is_last_measure_of_loop =
                        (e_header_ >= 0 && header.number == e_header_);
                    const int open_number =
                        song_.measure_headers[static_cast<std::size_t>(repeat_start_index_)].number;
                    const bool repeat_open_inside_loop = (open_number > s_header_);
                    const bool ignore_repeat_close =
                        is_last_measure_of_loop && !repeat_open_inside_loop;

                    if (repeat_open_ && header.repeat_close > 0 && !ignore_repeat_close) {
                        if (repeat_number_ < header.repeat_close || repeat_alternative_ > 0) {
                            repeat_end_ = header.start + header.length();
                            repeat_move_ += repeat_end_ - repeat_start_;
                            index_ = repeat_start_index_ - 1;
                            ++repeat_number_;
                        } else {
                            repeat_start_ = 0;
                            repeat_number_ = 0;
                            repeat_end_ = 0;
                            repeat_open_ = false;
                        }
                        repeat_alternative_ = 0;
                    }
                }
                ++index_;

                if ((s_header_ >= 0 && header.number < s_header_) ||
                    (e_header_ >= 0 && header.number > e_header_)) {
                    should_play_ = false;
                }
                if (s_header_ >= 0 && header.number < s_header_) {
                    repeat_move_ = 0;
                }
            }

            [[nodiscard]] bool finished() const noexcept { return index_ >= count_; }
            [[nodiscard]] bool should_play() const noexcept { return should_play_; }
            [[nodiscard]] int index() const noexcept { return index_; }
            [[nodiscard]] int repeat_number() const noexcept { return repeat_number_; }
            [[nodiscard]] long repeat_move() const noexcept { return repeat_move_; }

          private:
            const Song &song_;
            int count_{0};
            int index_{0};
            int last_index_{-1};
            bool should_play_{true};
            bool repeat_open_{true};
            long repeat_start_{kQuarterTime};
            long repeat_end_{0};
            long repeat_move_{0};
            int repeat_start_index_{0};
            int repeat_number_{0};
            int repeat_alternative_{0};
            int s_header_{-1};
            int e_header_{-1};
        };

    } // namespace

    std::vector<PlaybackMeasure> expand_repeats(const Song &song, const int s_header,
                                                const int e_header) {
        std::vector<PlaybackMeasure> playback;
        RepeatController controller(song, s_header, e_header);
        while (!controller.finished()) {
            const PlaybackMeasure step{.header_index = controller.index(),
                                       .pass = controller.repeat_number(),
                                       .repeat_move = controller.repeat_move()};
            controller.process();
            if (controller.should_play()) {
                playback.push_back(step);
            }
        }
        return playback;
    }

} // namespace libgp_parser
