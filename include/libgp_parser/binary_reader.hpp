#pragma once

#include "libgp_parser/error.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace libgp_parser {

    /// Reads GTP binary data little-endian from a buffer.
    /// @par TuxGuitar source
    /// common/TuxGuitar-gtp/.../GTPInputStream.java (readInt, readStringByte, …).
    /// @par Brief
    /// Port of protected read* methods for GP3–GP5.
    /// @par Visibility
    /// public (class).
    class BinaryReader {
      public:
        /// Creates a reader over a full in-memory file.
        /// @par Brief Sets read position to 0.
        /// @par Visibility public.
        explicit BinaryReader(std::vector<std::uint8_t> data);

        /// Current read position in the buffer.
        /// @par Visibility public.
        [[nodiscard]] std::size_t position() const noexcept { return pos_; }
        /// Buffer size in bytes.
        /// @par Visibility public.
        [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
        /// true when end of file is reached.
        /// @par Visibility public.
        [[nodiscard]] bool eof() const noexcept { return pos_ >= data_.size(); }

        /// Sets the read position (for rewinding a failed beat).
        /// @par Brief Fails if pos is past the end of the buffer.
        /// @par Visibility public.
        [[nodiscard]] ParseResult<Ok> seek(std::size_t pos);

        /// Reads one byte (readUnsignedByte).
        /// @par TuxGuitar source readUnsignedByte().
        /// @par Visibility public.
        [[nodiscard]] ParseResult<std::uint8_t> read_u8();
        /// Reads a signed byte (readByte).
        /// @par TuxGuitar source readByte().
        /// @par Visibility public.
        [[nodiscard]] ParseResult<std::int8_t> read_i8();
        /// Reads a 32-bit signed int (readInt).
        /// @par TuxGuitar source readInt().
        /// @par Visibility public.
        [[nodiscard]] ParseResult<std::int32_t> read_i32();
        /// Reads boolean (1 = true).
        /// @par TuxGuitar source readBoolean().
        /// @par Visibility public.
        [[nodiscard]] ParseResult<bool> read_bool();

        /// Skips n bytes (skip).
        /// @par TuxGuitar source skip(bytes).
        /// @par Visibility public.
        [[nodiscard]] ParseResult<Ok> skip(std::size_t count);

        /// Reads the GTP version string at the start of the file.
        /// @par TuxGuitar source GTPFileFormatDetector / readVersion.
        /// @par Visibility public.
        [[nodiscard]] ParseResult<std::string> read_gtp_version();

        /// Reads a string with length readInt()-1 (readStringByteSizeOfInteger).
        /// @par TuxGuitar source readStringByteSizeOfInteger().
        /// @par Visibility public.
        [[nodiscard]] ParseResult<std::string> read_string_byte_size_of_integer();

        /// Reads a string with length readUnsignedByte()-1 (readStringByteSizeOfByte).
        /// @par TuxGuitar source readStringByteSizeOfByte() — used by GP1/GP2 info fields.
        /// @par Visibility public.
        [[nodiscard]] ParseResult<std::string> read_string_byte_size_of_byte();

        /// Reads a string with length readInt() (readStringInteger).
        /// @par TuxGuitar source readStringInteger().
        /// @par Visibility public.
        [[nodiscard]] ParseResult<std::string> read_string_integer();

        /// Skips page setup (GP5).
        /// @par TuxGuitar source readPageSetup().
        /// @par Visibility public.
        [[nodiscard]] ParseResult<Ok> skip_page_setup(int version_code);

        /// Reads readStringByte(fixed size).
        /// @par TuxGuitar source readStringByte(size).
        /// @par Visibility public.
        [[nodiscard]] ParseResult<std::string> read_string_byte(int size);

        /// Skips readStringByte without decoding text.
        /// @par Brief Advances position only.
        /// @par Visibility public.
        [[nodiscard]] ParseResult<Ok> skip_string_byte(int size);

      private:
        /// Internal string helper (readString).
        /// @par TuxGuitar source readString(size, len).
        /// @par Visibility private.
        [[nodiscard]] ParseResult<std::string> read_string(int size, int len);

        std::vector<std::uint8_t> data_;
        std::size_t pos_{0};
    };

} // namespace libgp_parser
