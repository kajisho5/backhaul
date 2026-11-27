#include "core/sha256.h"

#include <cstring>

namespace backhaul {
namespace {

constexpr std::array<std::uint32_t, 64> kRound = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u};

constexpr std::size_t kBlock = 64;

std::uint32_t rotr(std::uint32_t x, unsigned n) { return (x >> n) | (x << (32u - n)); }

class Hasher {
 public:
  void update(const std::uint8_t* data, std::size_t size) {
    total_ += size;
    while (size > 0) {
      const std::size_t take = (size < kBlock - fill_) ? size : kBlock - fill_;
      std::memcpy(block_.data() + fill_, data, take);
      fill_ += take;
      data += take;
      size -= take;
      if (fill_ == kBlock) {
        compress();
        fill_ = 0;
      }
    }
  }

  Sha256Digest finish() {
    const std::uint64_t bits = static_cast<std::uint64_t>(total_) * 8u;
    const std::uint8_t one = 0x80u;
    update(&one, 1);
    const std::uint8_t zero = 0u;
    while (fill_ != kBlock - 8) update(&zero, 1);
    std::array<std::uint8_t, 8> length{};
    for (std::size_t i = 0; i < 8; ++i) {
      length[i] = static_cast<std::uint8_t>((bits >> (8u * (7u - i))) & 0xffu);
    }
    update(length.data(), length.size());

    Sha256Digest out{};
    for (std::size_t i = 0; i < 8; ++i) {
      for (std::size_t b = 0; b < 4; ++b) {
        out[i * 4 + b] = static_cast<std::uint8_t>((state_[i] >> (8u * (3u - b))) & 0xffu);
      }
    }
    return out;
  }

 private:
  void compress() {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) {
      w[i] = (static_cast<std::uint32_t>(block_[i * 4]) << 24) |
             (static_cast<std::uint32_t>(block_[i * 4 + 1]) << 16) |
             (static_cast<std::uint32_t>(block_[i * 4 + 2]) << 8) |
             static_cast<std::uint32_t>(block_[i * 4 + 3]);
    }
    for (std::size_t i = 16; i < 64; ++i) {
      const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::array<std::uint32_t, 8> v = state_;
    for (std::size_t i = 0; i < 64; ++i) {
      const std::uint32_t s1 = rotr(v[4], 6) ^ rotr(v[4], 11) ^ rotr(v[4], 25);
      const std::uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
      const std::uint32_t t1 = v[7] + s1 + ch + kRound[i] + w[i];
      const std::uint32_t s0 = rotr(v[0], 2) ^ rotr(v[0], 13) ^ rotr(v[0], 22);
      const std::uint32_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
      const std::uint32_t t2 = s0 + maj;
      v[7] = v[6];
      v[6] = v[5];
      v[5] = v[4];
      v[4] = v[3] + t1;
      v[3] = v[2];
      v[2] = v[1];
      v[1] = v[0];
      v[0] = t1 + t2;
    }
    for (std::size_t i = 0; i < 8; ++i) state_[i] += v[i];
  }

  std::array<std::uint32_t, 8> state_ = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                         0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
  std::array<std::uint8_t, kBlock> block_{};
  std::size_t fill_ = 0;
  std::size_t total_ = 0;
};

const std::uint8_t* bytes(std::string_view text) {
  return reinterpret_cast<const std::uint8_t*>(text.data());
}

}  // namespace

Sha256Digest sha256(const std::uint8_t* data, std::size_t size) {
  Hasher h;
  h.update(data, size);
  return h.finish();
}

Sha256Digest sha256(std::string_view text) { return sha256(bytes(text), text.size()); }

Sha256Digest hmacSha256(std::string_view key, std::string_view message) {
  std::array<std::uint8_t, kBlock> k{};
  if (key.size() > kBlock) {
    const Sha256Digest d = sha256(key);
    std::memcpy(k.data(), d.data(), d.size());
  } else {
    std::memcpy(k.data(), key.data(), key.size());
  }
  std::array<std::uint8_t, kBlock> inner{};
  std::array<std::uint8_t, kBlock> outer{};
  for (std::size_t i = 0; i < kBlock; ++i) {
    inner[i] = static_cast<std::uint8_t>(k[i] ^ 0x36u);
    outer[i] = static_cast<std::uint8_t>(k[i] ^ 0x5cu);
  }
  Hasher hi;
  hi.update(inner.data(), inner.size());
  hi.update(bytes(message), message.size());
  const Sha256Digest innerDigest = hi.finish();
  Hasher ho;
  ho.update(outer.data(), outer.size());
  ho.update(innerDigest.data(), innerDigest.size());
  return ho.finish();
}

}  // namespace backhaul
