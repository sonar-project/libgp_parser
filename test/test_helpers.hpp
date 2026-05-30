#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace libgp_parser::test {

    inline std::filesystem::path test_data_dir() { return {LIBGP_PARSER_TEST_DATA_DIR}; }

    inline std::string read_fixture(const std::string &filename) {
        const auto path = test_data_dir() / filename;
        std::ifstream inputStream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(inputStream), std::istreambuf_iterator<char>()};
    }

} // namespace libgp_parser::test
