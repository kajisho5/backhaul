#include "core/sdk_token.h"

#include "core/sha256.h"

namespace backhaul {
namespace {

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

bool isPlainIdChar(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
         c == '_';
}

}  // namespace

std::string base64Url(const std::uint8_t* data, std::size_t size) {
  std::string out;
  out.reserve((size * 4 + 2) / 3);
  std::size_t i = 0;
  for (; i + 2 < size; i += 3) {
    const std::uint32_t n = (static_cast<std::uint32_t>(data[i]) << 16) |
                            (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                            static_cast<std::uint32_t>(data[i + 2]);
    out.push_back(kAlphabet[(n >> 18) & 63u]);
    out.push_back(kAlphabet[(n >> 12) & 63u]);
    out.push_back(kAlphabet[(n >> 6) & 63u]);
    out.push_back(kAlphabet[n & 63u]);
  }
  if (size - i == 1) {
    const std::uint32_t n = static_cast<std::uint32_t>(data[i]) << 16;
    out.push_back(kAlphabet[(n >> 18) & 63u]);
    out.push_back(kAlphabet[(n >> 12) & 63u]);
  } else if (size - i == 2) {
    const std::uint32_t n = (static_cast<std::uint32_t>(data[i]) << 16) |
                            (static_cast<std::uint32_t>(data[i + 1]) << 8);
    out.push_back(kAlphabet[(n >> 18) & 63u]);
    out.push_back(kAlphabet[(n >> 12) & 63u]);
    out.push_back(kAlphabet[(n >> 6) & 63u]);
  }
  return out;
}

std::string base64Url(std::string_view text) {
  return base64Url(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

std::string makeSdkToken(std::string_view clientId, std::string_view clientSecret,
                         std::int64_t issuedAtUnixSeconds, std::int64_t lifetimeSeconds) {
  if (clientId.empty() || clientSecret.empty()) return {};
  for (const char c : clientId) {
    if (!isPlainIdChar(c)) return {};
  }
  if (issuedAtUnixSeconds < 0) return {};
  if (lifetimeSeconds < kMinTokenLifetimeSeconds || lifetimeSeconds > kMaxTokenLifetimeSeconds) {
    return {};
  }
  const std::string expires = std::to_string(issuedAtUnixSeconds + lifetimeSeconds);

  std::string payload = "{\"appKey\":\"";
  payload.append(clientId);
  payload.append("\",\"iat\":");
  payload.append(std::to_string(issuedAtUnixSeconds));
  payload.append(",\"exp\":");
  payload.append(expires);
  payload.append(",\"tokenExp\":");
  payload.append(expires);
  payload.append("}");

  std::string signedPart = base64Url("{\"alg\":\"HS256\",\"typ\":\"JWT\"}");
  signedPart.push_back('.');
  signedPart.append(base64Url(payload));

  const Sha256Digest mac = hmacSha256(clientSecret, signedPart);
  std::string token = signedPart;
  token.push_back('.');
  token.append(base64Url(mac.data(), mac.size()));
  return token;
}

}  // namespace backhaul
