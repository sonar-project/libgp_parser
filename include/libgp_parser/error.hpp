#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace libgp_parser {

    /// Success with no return value (C++ stand-in for a void result).
    /// @par TuxGuitar source No Java equivalent; libgp_parser error handling.
    /// @par Brief Marker type for ParseResult&lt;Ok&gt;.
    /// @par Visibility public.
    struct Ok {};

    enum class ParseErrorCode : std::uint8_t {
        Io,
        Zip,
        Xml,
        NotFound,
        Unsupported,
    };

    struct ParseError {
        ParseErrorCode code{ParseErrorCode::Io};
        std::string message;

        [[nodiscard]] bool operator==(const ParseError &) const = default;
    };

    /// Result of a parse step: value or error (instead of a Java exception).
    /// @par TuxGuitar source Replaces TGFileFormatException / GPXFormatException.
    /// @par Brief Optional value plus ParseError.
    /// @par Visibility public (class).
    template <typename T> class ParseResult {
      public:
        /// Builds a successful result.
        /// @par Visibility public.
        [[nodiscard]] static ParseResult success(T value) {
            ParseResult result;
            result.value_ = std::move(value);
            return result;
        }

        /// Builds a failed result.
        /// @par Visibility public.
        [[nodiscard]] static ParseResult failure(ParseError error) {
            ParseResult result;
            result.error_ = std::move(error);
            return result;
        }

        [[nodiscard]] bool has_value() const noexcept { return value_.has_value(); }
        [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

        [[nodiscard]] T &value() & { return value_.value(); }
        [[nodiscard]] const T &value() const & { return value_.value(); }
        [[nodiscard]] T &&value() && { return std::move(value_.value()); }

        [[nodiscard]] const ParseError &error() const noexcept { return error_; }

      private:
        std::optional<T> value_;
        ParseError error_;
    };

} // namespace libgp_parser
