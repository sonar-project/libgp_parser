#include "libgp_parser/binary_reader.hpp"
#include "libgp_parser/bit_constants.hpp"

#include <cstring>
#include <span>
#include <string_view>

namespace libgp_parser {
    namespace {

        ParseError io_error(std::string_view msg) { return {ParseErrorCode::Io, std::string(msg)}; }

    } // namespace

    BinaryReader::BinaryReader(std::vector<std::uint8_t> data) : data_(std::move(data)) {}

    ParseResult<std::uint8_t> BinaryReader::read_u8() {
        if (eof()) {
            return ParseResult<std::uint8_t>::failure(io_error("Unexpected end of file"));
        }
        return ParseResult<std::uint8_t>::success(data_[pos_++]);
    }

    ParseResult<std::int32_t> BinaryReader::read_i32() {
        if (pos_ + 4 > data_.size()) {
            return ParseResult<std::int32_t>::failure(io_error("Unexpected end of file"));
        }
        const auto bytes = std::span{data_}.subspan(pos_, 4);
        pos_ += 4;
        return ParseResult<std::int32_t>::success(static_cast<std::int32_t>(
            bytes[0] | (bytes[1] << Constants::ShiftByte8) | (bytes[2] << Constants::ShiftByte16) |
            (bytes[3] << Constants::ShiftByte24)));
    }

    ParseResult<bool> BinaryReader::read_bool() {
        auto byte = read_u8();
        if (!byte) {
            return ParseResult<bool>::failure(byte.error());
        }
        return ParseResult<bool>::success(byte.value() == 1);
    }

    ParseResult<Ok> BinaryReader::skip(const std::size_t count) {
        if (pos_ + count > data_.size()) {
            return ParseResult<Ok>::failure(io_error("Unexpected end of file"));
        }
        pos_ += count;
        return ParseResult<Ok>::success(Ok{});
    }

    ParseResult<std::string> BinaryReader::read_gtp_version() {
        auto len_byte = read_u8();
        if (!len_byte) {
            return ParseResult<std::string>::failure(len_byte.error());
        }
        const int str_len = (len_byte.value() <= 30) ? static_cast<int>(len_byte.value()) : 30;
        if (pos_ + Constants::ShiftByte30 > data_.size()) {
            return ParseResult<std::string>::failure(io_error("Unexpected end of file"));
        }
        std::string version(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                            data_.begin() + static_cast<std::ptrdiff_t>(pos_) +
                                Constants::ShiftByte30);
        pos_ += Constants::ShiftByte30;
        version.resize(static_cast<std::size_t>(str_len));
        return ParseResult<std::string>::success(std::move(version));
    }

    ParseResult<std::string> BinaryReader::read_string(const int size, int len) {
        const int buffer_size = (size > 0 ? size : len);
        if (buffer_size < 0 || pos_ + static_cast<std::size_t>(buffer_size) > data_.size()) {
            return ParseResult<std::string>::failure(io_error("Unexpected end of file"));
        }
        if (len < 0 || len > buffer_size) {
            len = size;
        }
        std::string payload(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                            data_.begin() + static_cast<std::ptrdiff_t>(pos_) +
                                static_cast<std::ptrdiff_t>(len));
        pos_ += static_cast<std::size_t>(buffer_size);
        return ParseResult<std::string>::success(std::move(payload));
    }

    ParseResult<std::string> BinaryReader::read_string_byte(const int size) {
        auto len_byte = read_u8();
        if (!len_byte.has_value()) {
            return ParseResult<std::string>::failure(len_byte.error());
        }
        return read_string(size, static_cast<int>(len_byte.value()));
    }

    ParseResult<std::string> BinaryReader::read_string_byte_size_of_integer() {
        auto size_word = read_i32();
        if (!size_word.has_value()) {
            return ParseResult<std::string>::failure(size_word.error());
        }
        return read_string_byte(size_word.value() - 1);
    }

    ParseResult<std::string> BinaryReader::read_string_integer() {
        auto len = read_i32();
        if (!len.has_value()) {
            return ParseResult<std::string>::failure(len.error());
        }
        const auto num = len.value();
        if (num < 0 || pos_ + static_cast<std::size_t>(num) > data_.size()) {
            return ParseResult<std::string>::failure(io_error("Invalid string length"));
        }
        std::string payload(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                            data_.begin() + static_cast<std::ptrdiff_t>(pos_) +
                                static_cast<std::ptrdiff_t>(num));
        pos_ += static_cast<std::size_t>(num);
        return ParseResult<std::string>::success(std::move(payload));
    }

    ParseResult<Ok> BinaryReader::skip_lyrics() {
        if (auto from = read_i32(); !from.has_value()) {
            return ParseResult<Ok>::failure(from.error());
        }
        auto lyrics = read_string_integer();
        if (!lyrics.has_value()) {
            return ParseResult<Ok>::failure(lyrics.error());
        }
        for (int i = 0; i < 4; ++i) {
            if (auto marker = read_i32(); !marker.has_value()) {
                return ParseResult<Ok>::failure(marker.error());
            }
            auto chunk = read_string_integer();
            if (!chunk.has_value()) {
                return ParseResult<Ok>::failure(chunk.error());
            }
        }
        return ParseResult<Ok>::success(Ok{});
    }

    ParseResult<Ok> BinaryReader::skip_string_byte(const int size) {
        auto len_byte = read_u8();
        if (!len_byte.has_value()) {
            return ParseResult<Ok>::failure(len_byte.error());
        }
        const int len = static_cast<int>(len_byte.value());
        const int buffer_size = (size > 0 ? size : len);
        return skip(static_cast<std::size_t>(buffer_size));
    }

    ParseResult<Ok> BinaryReader::skip_page_setup(const int version_code) {
        const int header_skip = (version_code > 0) ? 49 : 30;
        if (auto result = skip(static_cast<std::size_t>(header_skip)); !result.has_value()) {
            return result;
        }
        for (int i = 0; i < Constants::ShiftByte11; ++i) {
            if (auto result = skip(4); !result.has_value()) {
                return result;
            }
            if (auto result = skip_string_byte(0); !result.has_value()) {
                return result;
            }
        }
        return ParseResult<Ok>::success(Ok{});
    }

    ParseResult<Ok> BinaryReader::skip_channels() {
        for (int i = 0; i < Constants::ShiftByte64; ++i) {
            if (auto result = skip(4 + Constants::ShiftByte6); !result.has_value()) {
                return result;
            }
            if (auto result = skip(2); !result.has_value()) {
                return result;
            }
        }
        return ParseResult<Ok>::success(Ok{});
    }

} // namespace libgp_parser
