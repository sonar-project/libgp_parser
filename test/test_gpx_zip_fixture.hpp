#pragma once

#include "test_helpers.hpp"

#include <miniz.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>

namespace libgp_parser::test {

    inline std::filesystem::path write_temp_gpx_fixture() {
        const std::string score_xml = read_fixture("minimal_score.gpif");
        constexpr int initial_heap_size = 1024;

        mz_zip_archive zip{};
        std::memset(&zip, 0, sizeof(zip));
        mz_zip_writer_init_heap(&zip, 0, initial_heap_size);
        mz_zip_writer_add_mem(&zip, "Content/score.gpif", score_xml.data(), score_xml.size(),
                              MZ_DEFAULT_COMPRESSION);
        mz_zip_writer_add_mem(&zip, "VERSION", "7.0", 3, MZ_NO_COMPRESSION);

        void *heap = nullptr;
        size_t size = 0;
        mz_zip_writer_finalize_heap_archive(&zip, &heap, &size);
        mz_zip_writer_end(&zip);

        const std::span<const std::uint8_t> bytes{static_cast<const std::uint8_t *>(heap), size};
        const std::string payload(bytes.begin(), bytes.end());

        const auto temp_dir = std::filesystem::temp_directory_path();
        const auto path = temp_dir / "libgp_parser_test.gpx";
        std::ofstream out(path, std::ios::binary);
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        mz_free(heap);
        return path;
    }

} // namespace libgp_parser::test
