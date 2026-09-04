#include "libgp_parser/gpx_edit_lock.hpp"

#include <miniz.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <algorithm>

namespace libgp_parser {
    namespace {

        // Static salt from Guitar Pro's edit-lock key derivation (libGPIO).
        constexpr std::uint8_t kEditLockSalt[] = {0xda, 0x40, 0xcc, 0x64, 0x90, 0x0b, 0x61, 0x7a,
                                                  0x0f, 0x72, 0xad, 0x4e, 0x6e, 0xf4, 0x2f, 0x9c};
        constexpr int kPbkdf2Iterations = 4096;
        constexpr int kAesKeyLen = 32;
        constexpr int kAesIvLen = 16;

        [[nodiscard]] bool starts_with_ascii(std::span<const std::uint8_t> bytes,
                                             std::string_view prefix) noexcept {
            if (bytes.size() < prefix.size()) {
                return false;
            }
            return std::equal(prefix.begin(), prefix.end(), bytes.begin(),
                              [](char expected, std::uint8_t actual) {
                                  return static_cast<unsigned char>(expected) == actual;
                              });
        }

        [[nodiscard]] ParseResult<std::vector<std::uint8_t>>
        zlib_inflate(std::span<const std::uint8_t> compressed) {
            size_t out_len = 0;
            void *out = tinfl_decompress_mem_to_heap(
                compressed.data(), compressed.size(), &out_len, TINFL_FLAG_PARSE_ZLIB_HEADER);
            if (out == nullptr) {
                return ParseResult<std::vector<std::uint8_t>>::failure(
                    {.code = ParseErrorCode::Zip,
                     .message = "Failed to zlib-decompress edit-locked score.gpif"});
            }
            std::vector<std::uint8_t> result(static_cast<const std::uint8_t *>(out),
                                             static_cast<const std::uint8_t *>(out) + out_len);
            mz_free(out);
            return ParseResult<std::vector<std::uint8_t>>::success(std::move(result));
        }

        [[nodiscard]] ParseResult<std::vector<std::uint8_t>>
        aes256_cbc_decrypt(std::span<const std::uint8_t> key, std::span<const std::uint8_t> iv,
                           std::span<const std::uint8_t> ciphertext) {
            if (key.size() != static_cast<std::size_t>(kAesKeyLen) ||
                iv.size() != static_cast<std::size_t>(kAesIvLen)) {
                return ParseResult<std::vector<std::uint8_t>>::failure(
                    {.code = ParseErrorCode::Unsupported,
                     .message = "Invalid AES key or IV length for edit-locked score.gpif"});
            }
            if (ciphertext.empty() || (ciphertext.size() % 16) != 0) {
                return ParseResult<std::vector<std::uint8_t>>::failure(
                    {.code = ParseErrorCode::Unsupported,
                     .message = "edit-locked score.gpif ciphertext has invalid length"});
            }

            EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
            if (ctx == nullptr) {
                return ParseResult<std::vector<std::uint8_t>>::failure(
                    {.code = ParseErrorCode::Unsupported,
                     .message = "OpenSSL EVP_CIPHER_CTX_new failed"});
            }

            std::vector<std::uint8_t> plaintext(ciphertext.size() + 16);
            int out_len = 0;
            int final_len = 0;
            bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) ==
                          1 &&
                      EVP_DecryptUpdate(ctx, plaintext.data(), &out_len, ciphertext.data(),
                                        static_cast<int>(ciphertext.size())) == 1 &&
                      EVP_DecryptFinal_ex(ctx, plaintext.data() + out_len, &final_len) == 1;
            EVP_CIPHER_CTX_free(ctx);

            if (!ok) {
                return ParseResult<std::vector<std::uint8_t>>::failure(
                    {.code = ParseErrorCode::Unsupported,
                     .message = "AES decryption of edit-locked score.gpif failed"});
            }

            plaintext.resize(static_cast<std::size_t>(out_len + final_len));
            return ParseResult<std::vector<std::uint8_t>>::success(std::move(plaintext));
        }

    } // namespace

    bool looks_like_gpif_xml(const std::span<const std::uint8_t> bytes) noexcept {
        std::size_t offset = 0;
        if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
            offset = 3;
        } else if (bytes.size() >= 2 &&
                   ((bytes[0] == 0xFF && bytes[1] == 0xFE) || (bytes[0] == 0xFE && bytes[1] == 0xFF))) {
            return true;
        }

        while (offset < bytes.size() &&
               (bytes[offset] == ' ' || bytes[offset] == '\t' || bytes[offset] == '\n' ||
                bytes[offset] == '\r')) {
            ++offset;
        }
        if (offset >= bytes.size() || bytes[offset] != '<') {
            return false;
        }

        const auto rest = bytes.subspan(offset);
        // UTF-16 LE/BE without BOM: '<' followed by NUL (or NUL then '?').
        if (rest.size() >= 2 && rest[1] == 0) {
            return true;
        }
        return starts_with_ascii(rest, "<?xml") || starts_with_ascii(rest, "<GPIF") ||
               starts_with_ascii(rest, "<gpif");
    }

    ParseResult<std::vector<std::uint8_t>>
    decrypt_edit_locked_gpif(const std::span<const std::uint8_t> encrypted_gpif,
                             const std::span<const std::uint8_t> edit_locked_password) {
        if (edit_locked_password.empty()) {
            return ParseResult<std::vector<std::uint8_t>>::failure(
                {.code = ParseErrorCode::Unsupported, .message = "editLocked entry is empty"});
        }
        if (encrypted_gpif.size() < static_cast<std::size_t>(kAesIvLen) + 16) {
            return ParseResult<std::vector<std::uint8_t>>::failure(
                {.code = ParseErrorCode::Unsupported,
                 .message = "edit-locked score.gpif is too short"});
        }

        std::uint8_t key[kAesKeyLen];
        if (PKCS5_PBKDF2_HMAC(reinterpret_cast<const char *>(edit_locked_password.data()),
                              static_cast<int>(edit_locked_password.size()), kEditLockSalt,
                              static_cast<int>(sizeof(kEditLockSalt)), kPbkdf2Iterations, EVP_sha1(),
                              kAesKeyLen, key) != 1) {
            return ParseResult<std::vector<std::uint8_t>>::failure(
                {.code = ParseErrorCode::Unsupported,
                 .message = "PBKDF2 key derivation for edit-locked score.gpif failed"});
        }

        const auto iv = encrypted_gpif.first(static_cast<std::size_t>(kAesIvLen));
        const auto ciphertext = encrypted_gpif.subspan(static_cast<std::size_t>(kAesIvLen));
        auto plaintext = aes256_cbc_decrypt(key, iv, ciphertext);
        OPENSSL_cleanse(key, sizeof(key));
        if (!plaintext) {
            return plaintext;
        }

        // Guitar Pro stores zlib-wrapped XML after AES; accept raw XML as a fallback.
        if (looks_like_gpif_xml(plaintext.value())) {
            return plaintext;
        }
        if (!plaintext.value().empty() && plaintext.value().front() == 0x78) {
            return zlib_inflate(plaintext.value());
        }
        auto inflated = zlib_inflate(plaintext.value());
        if (inflated) {
            return inflated;
        }
        return ParseResult<std::vector<std::uint8_t>>::failure(
            {.code = ParseErrorCode::Unsupported,
             .message = "Decrypted score.gpif is not valid zlib/XML"});
    }

} // namespace libgp_parser
