#include <libgp_parser/byte_string.hpp>
#include <libgp_parser/gpx_archive.hpp>
#include <libgp_parser/gpx_document.hpp>
#include <libgp_parser/gpx_edit_lock.hpp>
#include <libgp_parser/gpx_score_bundle.hpp>
#include <libgp_parser/load_song.hpp>

#include "test_helpers.hpp"

#include <catch2/catch_test_macros.hpp>
#include <miniz.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

using namespace libgp_parser;

namespace {

    constexpr int initial_zip_size = 1024;
    constexpr std::uint8_t kEditLockSalt[] = {0xda, 0x40, 0xcc, 0x64, 0x90, 0x0b, 0x61, 0x7a,
                                              0x0f, 0x72, 0xad, 0x4e, 0x6e, 0xf4, 0x2f, 0x9c};

    std::vector<std::uint8_t> make_gpx_zip(const std::string &version) {
        const std::string score_xml = test::read_fixture("minimal_score.gpif");

        mz_zip_archive zip{};
        std::memset(&zip, 0, sizeof(zip));
        if (mz_zip_writer_init_heap(&zip, 0, initial_zip_size) == 0) {
            return {};
        }

        mz_zip_writer_add_mem(&zip, "Content/score.gpif", score_xml.data(), score_xml.size(),
                              MZ_DEFAULT_COMPRESSION);
        mz_zip_writer_add_mem(&zip, "VERSION", version.data(), version.size(), MZ_NO_COMPRESSION);

        void *heap = nullptr;
        size_t size = 0;
        if (mz_zip_writer_finalize_heap_archive(&zip, &heap, &size) == 0) {
            mz_zip_writer_end(&zip);
            return {};
        }
        mz_zip_writer_end(&zip);

        const std::span<const std::uint8_t> bytes{static_cast<const std::uint8_t *>(heap), size};
        std::vector<std::uint8_t> out(bytes.begin(), bytes.end());
        mz_free(heap);
        return out;
    }

    std::vector<std::uint8_t> make_minimal_gpx_zip() { return make_gpx_zip("7.0"); }

    std::vector<std::uint8_t> zlib_compress(std::span<const std::uint8_t> input) {
        mz_ulong bound = mz_compressBound(static_cast<mz_ulong>(input.size()));
        std::vector<std::uint8_t> out(static_cast<std::size_t>(bound));
        mz_ulong out_len = bound;
        REQUIRE(mz_compress(out.data(), &out_len, input.data(),
                            static_cast<mz_ulong>(input.size())) == MZ_OK);
        out.resize(static_cast<std::size_t>(out_len));
        return out;
    }

    std::vector<std::uint8_t> aes256_cbc_encrypt(std::span<const std::uint8_t> key,
                                                 std::span<const std::uint8_t> iv,
                                                 std::span<const std::uint8_t> plaintext) {
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        REQUIRE(ctx != nullptr);
        std::vector<std::uint8_t> ciphertext(plaintext.size() + 16);
        int out_len = 0;
        int final_len = 0;
        REQUIRE(EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) == 1);
        REQUIRE(EVP_EncryptUpdate(ctx, ciphertext.data(), &out_len, plaintext.data(),
                                  static_cast<int>(plaintext.size())) == 1);
        REQUIRE(EVP_EncryptFinal_ex(ctx, ciphertext.data() + out_len, &final_len) == 1);
        EVP_CIPHER_CTX_free(ctx);
        ciphertext.resize(static_cast<std::size_t>(out_len + final_len));
        return ciphertext;
    }

    std::vector<std::uint8_t> encrypt_edit_locked_gpif(std::span<const std::uint8_t> xml,
                                                       std::string_view password) {
        std::uint8_t key[32];
        REQUIRE(PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), kEditLockSalt,
                                  static_cast<int>(sizeof(kEditLockSalt)), 4096, EVP_sha1(), 32,
                                  key) == 1);

        std::uint8_t iv[16];
        REQUIRE(RAND_bytes(iv, sizeof(iv)) == 1);

        const auto compressed = zlib_compress(xml);
        const auto ciphertext = aes256_cbc_encrypt(key, iv, compressed);

        std::vector<std::uint8_t> encrypted;
        encrypted.reserve(sizeof(iv) + ciphertext.size());
        encrypted.insert(encrypted.end(), iv, iv + sizeof(iv));
        encrypted.insert(encrypted.end(), ciphertext.begin(), ciphertext.end());
        return encrypted;
    }

    std::vector<std::uint8_t>
    make_edit_locked_zip(std::string_view password, std::span<const std::uint8_t> encrypted_gpif,
                         bool include_edit_locked = true) {
        mz_zip_archive zip{};
        std::memset(&zip, 0, sizeof(zip));
        if (mz_zip_writer_init_heap(&zip, 0, initial_zip_size) == 0) {
            return {};
        }

        mz_zip_writer_add_mem(&zip, "Content/score.gpif", encrypted_gpif.data(),
                              encrypted_gpif.size(), MZ_NO_COMPRESSION);
        mz_zip_writer_add_mem(&zip, "VERSION", "7.0", 3, MZ_NO_COMPRESSION);
        if (include_edit_locked) {
            mz_zip_writer_add_mem(&zip, "editLocked", password.data(), password.size(),
                                  MZ_NO_COMPRESSION);
        }

        void *heap = nullptr;
        size_t size = 0;
        if (mz_zip_writer_finalize_heap_archive(&zip, &heap, &size) == 0) {
            mz_zip_writer_end(&zip);
            return {};
        }
        mz_zip_writer_end(&zip);

        const std::span<const std::uint8_t> bytes{static_cast<const std::uint8_t *>(heap), size};
        std::vector<std::uint8_t> out(bytes.begin(), bytes.end());
        mz_free(heap);
        return out;
    }

    std::filesystem::path write_temp_bytes(const std::vector<std::uint8_t> &bytes,
                                           const std::string &name) {
        const auto path = std::filesystem::temp_directory_path() / name;
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        return path;
    }

} // namespace

TEST_CASE("GpxZipArchive extracts Content/score.gpif", "[gpx][zip]") {
    const std::vector<std::uint8_t> zip_bytes = make_minimal_gpx_zip();
    REQUIRE_FALSE(zip_bytes.empty());

    auto archive = GpxZipArchive::open(zip_bytes);
    REQUIRE(archive);
    REQUIRE(archive.value().contains(kGpxScorePathGp7));

    auto entry = archive.value().extract(kGpxScorePathGp7);
    REQUIRE(entry);
    const std::string xml = bytes_to_string(entry.value());
    const std::string_view xml_view{xml};
    REQUIRE(xml_view.contains("<Title>Test Song</Title>"));
}

TEST_CASE("GpxZipArchive extract_score_gpif", "[gpx][zip]") {
    const std::vector<std::uint8_t> zip_bytes = make_minimal_gpx_zip();
    auto archive = GpxZipArchive::open(zip_bytes);
    REQUIRE(archive);

    auto score = archive.value().extract_score_gpif();
    REQUIRE(score);
    REQUIRE(score.value().size() > 0);
}

TEST_CASE("extract_score_gpif accepts GP7 VERSION 7.0", "[gpx][gp7]") {
    const std::vector<std::uint8_t> zip_bytes = make_gpx_zip("7.0");
    auto score = extract_score_gpif(zip_bytes);
    REQUIRE(score);
}

TEST_CASE("extract_score_gpif accepts GP8 VERSION 8.0", "[gpx][gp7]") {
    const std::vector<std::uint8_t> zip_bytes = make_gpx_zip("8.0");
    auto score = extract_score_gpif(zip_bytes);
    REQUIRE(score);
}

TEST_CASE("extract_score_gpif rejects unsupported GP ZIP VERSION", "[gpx][gp7]") {
    const std::vector<std::uint8_t> zip_bytes = make_gpx_zip("6.0");
    auto score = extract_score_gpif(zip_bytes);
    REQUIRE_FALSE(score);
    REQUIRE(score.error().code == ParseErrorCode::Unsupported);
}

TEST_CASE("looks_like_gpif_xml detects UTF-8 and UTF-16 BOM", "[gpx][editlock]") {
    const std::string utf8 = test::read_fixture("minimal_score.gpif");
    REQUIRE(looks_like_gpif_xml(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t *>(utf8.data()), utf8.size())));

    const std::uint8_t garbage[] = {0x01, 0x02, 0x03, 0x04};
    REQUIRE_FALSE(looks_like_gpif_xml(garbage));

    const std::uint8_t utf16_le_bom[] = {0xFF, 0xFE, '<', 0, 'G', 0};
    REQUIRE(looks_like_gpif_xml(utf16_le_bom));
}

TEST_CASE("decrypt_edit_locked_gpif round-trips zlib XML", "[gpx][editlock]") {
    const std::string xml = test::read_fixture("minimal_score.gpif");
    const std::string password = "deadbeef$0123456789abcdef0123456789abcdef";
    const auto encrypted = encrypt_edit_locked_gpif(
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(xml.data()),
                                      xml.size()),
        password);

    auto decrypted = decrypt_edit_locked_gpif(
        encrypted, std::span<const std::uint8_t>(
                       reinterpret_cast<const std::uint8_t *>(password.data()), password.size()));
    REQUIRE(decrypted);
    REQUIRE(looks_like_gpif_xml(decrypted.value()));
    REQUIRE(bytes_to_string(decrypted.value()) == xml);
}

TEST_CASE("extract_score_gpif unlocks edit-locked ZIP", "[gpx][editlock]") {
    const std::string xml = test::read_fixture("minimal_score.gpif");
    const std::string password = "cafebabe$fedcba9876543210fedcba9876543210";
    const auto encrypted = encrypt_edit_locked_gpif(
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(xml.data()),
                                      xml.size()),
        password);
    const auto zip_bytes = make_edit_locked_zip(password, encrypted);
    REQUIRE_FALSE(zip_bytes.empty());

    auto score = extract_score_gpif(zip_bytes);
    REQUIRE(score);
    REQUIRE(bytes_to_string(score.value()).contains("<Title>Test Song</Title>"));

    const auto path = write_temp_bytes(zip_bytes, "libgp_parser_edit_locked.gp");
    auto song = load_song(path);
    REQUIRE(song);
    REQUIRE(song.value().name() == "Test Song");
    REQUIRE(song.value().artist() == "Test Artist");
    std::error_code error_code;
    std::filesystem::remove(path, error_code);
}

TEST_CASE("extract_score_gpif reports non-XML when editLocked is missing", "[gpx][editlock]") {
    const std::uint8_t binary_payload[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
                                           0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71, 0x81,
                                           0x12, 0x22, 0x32, 0x42, 0x52, 0x62, 0x72, 0x82,
                                           0x13, 0x23, 0x33, 0x43, 0x53, 0x63, 0x73, 0x83};
    const auto zip_bytes = make_edit_locked_zip("unused", binary_payload, false);
    auto score = extract_score_gpif(zip_bytes);
    REQUIRE_FALSE(score);
    REQUIRE(score.error().code == ParseErrorCode::Unsupported);
    REQUIRE(score.error().message.contains("not XML"));
}

TEST_CASE("parse_gpx_document XML errors include offset", "[gpx][xml]") {
    auto document = parse_gpx_document("<GPIF><Score><Title>x</Title", false);
    REQUIRE_FALSE(document);
    REQUIRE(document.error().code == ParseErrorCode::Xml);
    REQUIRE(document.error().message.contains("at offset"));
}

TEST_CASE("parse_gpx_document accepts UTF-16 LE GPIF", "[gpx][xml]") {
    const std::string utf8 = "<?xml version=\"1.0\"?><GPIF><Score><Title>Utf16</Title>"
                             "<Artist></Artist><Album></Album></Score></GPIF>";
    std::string utf16;
    utf16.push_back(static_cast<char>(0xFF));
    utf16.push_back(static_cast<char>(0xFE));
    for (unsigned char ch : utf8) {
        utf16.push_back(static_cast<char>(ch));
        utf16.push_back('\0');
    }
    auto document = parse_gpx_document(utf16, true);
    REQUIRE(document);
    REQUIRE(document.value().score.title == "Utf16");
}
