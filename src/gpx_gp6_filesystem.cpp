#include "libgp_parser/gpx_gp6_filesystem.hpp"

#include "libgp_parser/gpx_archive.hpp"
#include "libgp_parser/gpx_format.hpp"

#include "libgp_parser/bit_constants.hpp"

#include <algorithm>
#include <cstring>
#include <span>
#include <utility> // std::cmp_less

namespace libgp_parser {
    namespace {

        class GpxBitBuffer {
          public:
            explicit GpxBitBuffer(std::vector<std::uint8_t> buffer) : buffer_(std::move(buffer)) {}

            [[nodiscard]] std::size_t length() const { return buffer_.size(); }
            [[nodiscard]] std::size_t offset() const { return position_ / Constants::ShiftByte8; }
            [[nodiscard]] bool end() const { return offset() >= length(); }

            int read_bit() {
                const std::size_t byte_index = position_ / 8;
                const int byte_offset = static_cast<int>(7 - (position_ % 8));
                if (byte_index >= buffer_.size()) {
                    return -1;
                }
                const int bit = (buffer_[byte_index] >> byte_offset) & 0x01;
                ++position_;
                return bit;
            }

            int read_bits(const int count) {
                int bits = 0;
                for (int i = count - 1; i >= 0; --i) {
                    bits |= (read_bit() << i);
                }
                return bits;
            }

            int read_bits_reversed(const int count) {
                int bits = 0;
                for (int i = 0; i < count; ++i) {
                    bits |= (read_bit() << i);
                }
                return bits;
            }

            std::vector<std::uint8_t> read_bytes(const int count) {
                std::vector<std::uint8_t> bytes(static_cast<std::size_t>(count));
                for (int i = 0; i < count; ++i) {
                    bytes[static_cast<std::size_t>(i)] =
                        static_cast<std::uint8_t>(read_bits(Constants::ShiftByte8));
                }
                return bytes;
            }

          private:
            std::vector<std::uint8_t> buffer_;
            std::size_t position_{0};
        };

        std::int32_t get_integer(std::span<const std::uint8_t> source, const std::size_t offset) {
            return static_cast<std::int32_t>(source[offset + 0] |
                                             (source[offset + 1] << Constants::ShiftByte8) |
                                             (source[offset + 2] << Constants::ShiftByte16) |
                                             (source[offset + 3] << Constants::ShiftByte24));
        }

        std::string get_string(std::span<const std::uint8_t> source, std::size_t offset,
                               const std::size_t length) {
            std::string result;
            result.reserve(length);
            for (std::size_t i = 0; i < length; ++i) {
                const int char_value = source[offset + i] & 0xff;
                if (char_value == 0) {
                    break;
                }
                result.push_back(static_cast<char>(char_value));
            }
            return result;
        }

        void load_bcfs(std::span<const std::uint8_t> bcfs_bytes,
                       std::vector<std::pair<std::string, std::vector<std::uint8_t>>> &out_files) {
            constexpr int kSectorSize = 0x1000;
            const std::size_t bcfs_len = bcfs_bytes.size();
            int offset = 0;
            while ((offset = offset + kSectorSize) + 3 < static_cast<int>(bcfs_len)) {
                if (get_integer(bcfs_bytes, static_cast<std::size_t>(offset)) == 2) {
                    const int index_file_name = offset + 4;
                    const int index_file_size = offset + 0x8C;
                    const int index_of_block = offset + 0x94;

                    int block = 0;
                    int block_count = 0;
                    std::vector<std::uint8_t> file_bytes_stream;
                    while ((block = get_integer(
                                bcfs_bytes, static_cast<std::size_t>(index_of_block) +
                                                (4U * static_cast<std::size_t>(block_count++)))) !=
                           0) {
                        offset = block * kSectorSize;
                        const std::size_t sector_end =
                            std::min(static_cast<std::size_t>(offset + kSectorSize), bcfs_len);
                        file_bytes_stream.insert(
                            file_bytes_stream.end(),
                            bcfs_bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                            bcfs_bytes.begin() + static_cast<std::ptrdiff_t>(sector_end));
                    }

                    const int file_size =
                        get_integer(bcfs_bytes, static_cast<std::size_t>(index_file_size));
                    if (static_cast<int>(file_bytes_stream.size()) >= file_size && file_size > 0) {
                        file_bytes_stream.resize(static_cast<std::size_t>(file_size));
                        const std::string name =
                            get_string(bcfs_bytes, static_cast<std::size_t>(index_file_name), 127);
                        out_files.emplace_back(name, std::move(file_bytes_stream));
                    }
                }
            }
        }

        ParseResult<std::vector<std::uint8_t>>
        decompress_bcfz(std::vector<std::uint8_t> compressed) {
            GpxBitBuffer src(std::move(compressed));
            if (src.length() < 4) {
                return ParseResult<std::vector<std::uint8_t>>::failure(
                    {.code = ParseErrorCode::Zip, .message = "BCFZ data too short"});
            }

            const auto expect_len_bytes = src.read_bytes(4);
            const int expect_length = get_integer(expect_len_bytes, 0);

            std::vector<std::uint8_t> bcfs_buffer;
            bcfs_buffer.reserve(static_cast<std::size_t>(expect_length));

            while (!src.end() && static_cast<int>(src.offset()) < expect_length) {
                const int flag = src.read_bits(1);
                if (flag == 1) {
                    const int bits = src.read_bits(4);
                    const int offs = src.read_bits_reversed(bits);
                    const int size = src.read_bits_reversed(bits);
                    const int pos = static_cast<int>(bcfs_buffer.size()) - offs;
                    const int copy_count = (size > offs ? offs : size);
                    for (int i = 0; i < copy_count; ++i) {
                        const int backref_index = pos + i;
                        if (backref_index < 0 ||
                            !std::cmp_less(backref_index, bcfs_buffer.size())) {
                            continue;
                        }
                        const std::size_t source_index = static_cast<std::size_t>(backref_index);
                        bcfs_buffer.push_back(bcfs_buffer[source_index]);
                    }
                } else {
                    const int size = src.read_bits_reversed(2);
                    for (int i = 0; i < size; ++i) {
                        bcfs_buffer.push_back(
                            static_cast<std::uint8_t>(src.read_bits(Constants::ShiftByte8)));
                    }
                }
            }

            return ParseResult<std::vector<std::uint8_t>>::success(std::move(bcfs_buffer));
        }

    } // namespace

    GpxGp6FileSystem::GpxGp6FileSystem(
        std::vector<std::pair<std::string, std::vector<std::uint8_t>>> files)
        : files_(std::move(files)) {}

    ParseResult<GpxGp6FileSystem> GpxGp6FileSystem::load(std::vector<std::uint8_t> data) {
        if (data.size() < 4) {
            return ParseResult<GpxGp6FileSystem>::failure(
                {.code = ParseErrorCode::Unsupported, .message = "GPX file too short"});
        }

        const std::uint32_t header = read_le_u32(data);
        if (header == kGpxHeaderBcfs) {
            std::vector<std::uint8_t> bcfs(data.begin() + 4, data.end());
            std::vector<std::pair<std::string, std::vector<std::uint8_t>>> files;
            load_bcfs(bcfs, files);
            return ParseResult<GpxGp6FileSystem>::success(GpxGp6FileSystem(std::move(files)));
        }

        if (header == kGpxHeaderBcfz) {
            auto bcfs = decompress_bcfz(std::vector<std::uint8_t>(data.begin() + 4, data.end()));
            if (!bcfs) {
                return ParseResult<GpxGp6FileSystem>::failure(bcfs.error());
            }
            const std::vector<std::uint8_t> &buf = bcfs.value();
            if (buf.size() < 4 || read_le_u32(buf) != kGpxHeaderBcfs) {
                return ParseResult<GpxGp6FileSystem>::failure(
                    {.code = ParseErrorCode::Zip, .message = "BCFZ payload is not a BCFS archive"});
            }
            std::vector<std::pair<std::string, std::vector<std::uint8_t>>> files;
            load_bcfs(std::span{buf}.subspan(4), files);
            return ParseResult<GpxGp6FileSystem>::success(GpxGp6FileSystem(std::move(files)));
        }

        return ParseResult<GpxGp6FileSystem>::failure(
            {.code = ParseErrorCode::Unsupported,
             .message = "Not a GP6 GPX container (expected BCFS or BCFZ header)"});
    }

    bool GpxGp6FileSystem::contains(const std::string_view file_name) const {
        return std::ranges::any_of(files_,
                                   [&](const auto &entry) { return entry.first == file_name; });
    }

    ParseResult<std::vector<std::uint8_t>>
    GpxGp6FileSystem::extract(const std::string_view file_name) const {
        for (const auto &[name, contents] : files_) {
            if (name == file_name) {
                return ParseResult<std::vector<std::uint8_t>>::success(contents);
            }
        }
        return ParseResult<std::vector<std::uint8_t>>::failure(
            {.code = ParseErrorCode::NotFound,
             .message = std::string("GP6 file not found: ") + std::string(file_name)});
    }

    ParseResult<std::vector<std::uint8_t>> GpxGp6FileSystem::extract_score_gpif() const {
        if (contains(kGpxScorePathGp6)) {
            return extract(kGpxScorePathGp6);
        }
        return ParseResult<std::vector<std::uint8_t>>::failure(
            {.code = ParseErrorCode::NotFound, .message = "score.gpif not found in GP6 container"});
    }

} // namespace libgp_parser
