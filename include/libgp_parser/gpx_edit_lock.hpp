#pragma once

#include "libgp_parser/error.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace libgp_parser {

    /// ZIP entry name for Guitar Pro edit-lock metadata.
    inline constexpr std::string_view kGpxEditLockedPath = "editLocked";

    /// Returns true when bytes look like GPIF XML (UTF-8/UTF-16, optional BOM).
    /// @par Brief Used to detect plaintext vs edit-locked score.gpif.
    /// @par Visibility public.
    [[nodiscard]] bool looks_like_gpif_xml(std::span<const std::uint8_t> bytes) noexcept;

    /// Decrypts an edit-locked score.gpif payload (AES-256-CBC + zlib).
    /// Password is the raw contents of the editLocked ZIP entry.
    /// @par Brief Guitar Pro 7/8 edit-lock obfuscation used for playback.
    /// @par Visibility public.
    [[nodiscard]] ParseResult<std::vector<std::uint8_t>>
    decrypt_edit_locked_gpif(std::span<const std::uint8_t> encrypted_gpif,
                             std::span<const std::uint8_t> edit_locked_password);

} // namespace libgp_parser
