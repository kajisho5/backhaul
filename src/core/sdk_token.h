// Builds the signed token (JWT, HS256) that the Zoom Meeting SDK asks for at start-up.
// Payload fields follow https://developers.zoom.us/docs/meeting-sdk/auth/ :
// appKey, iat, exp, tokenExp.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace backhaul {

std::string base64Url(const std::uint8_t* data, std::size_t size);
std::string base64Url(std::string_view text);

// Zoom accepts a lifetime between 30 minutes and 48 hours.
constexpr std::int64_t kMinTokenLifetimeSeconds = 1800;
constexpr std::int64_t kMaxTokenLifetimeSeconds = 48 * 3600;

// Returns an empty string when the inputs cannot produce a valid token:
// empty id or secret, an id containing anything but letters, digits, '-' or '_',
// or a lifetime outside the range above.
std::string makeSdkToken(std::string_view clientId, std::string_view clientSecret,
                         std::int64_t issuedAtUnixSeconds, std::int64_t lifetimeSeconds);

}  // namespace backhaul
