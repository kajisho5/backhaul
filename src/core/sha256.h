// SHA-256 and HMAC-SHA256 (FIPS 180-4, RFC 2104). Used to sign the Zoom SDK token.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace backhaul {

using Sha256Digest = std::array<std::uint8_t, 32>;

Sha256Digest sha256(const std::uint8_t* data, std::size_t size);
Sha256Digest sha256(std::string_view text);

Sha256Digest hmacSha256(std::string_view key, std::string_view message);

}  // namespace backhaul
