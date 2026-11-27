#include "core/test_tone.h"

#include <algorithm>
#include <cmath>

namespace backhaul {
namespace {

constexpr float kFadeStep = 1.0f / (TestTone::kRate * 0.005f);  // 5 ms from silent to full
constexpr float kTwoPi = 6.283185307179586f;
// The pink filter below has an RMS of about 0.20 for white noise in -1..1 (measured). Pink noise
// peaks about four times above its RMS, so it is scaled to an RMS of 1/4 and the rare
// sample above full scale is held there: the meter then reads the level that was set.
constexpr float kPinkScale = 0.25f / 0.20f;

}  // namespace

void TestTone::setKind(Kind kind) { kind_.store(kind == Kind::Pink ? 1 : 0, std::memory_order_relaxed); }

void TestTone::setLevelDb(float db) {
  amplitude_.store(std::pow(10.0f, std::clamp(db, kMinDb, kMaxDb) / 20.0f), std::memory_order_relaxed);
}

void TestTone::setChannels(std::uint32_t mask) { mask_.store(mask, std::memory_order_relaxed); }

float TestTone::next() {
  if (kind_.load(std::memory_order_relaxed) == 0) {
    const float v = std::sin(kTwoPi * 1000.0f * static_cast<float>(phase_) / static_cast<float>(kRate));
    phase_ = (phase_ + 1) % static_cast<std::uint32_t>(kRate);
    return v;
  }
  // White noise (xorshift) through Paul Kellett's pink filter.
  random_ ^= random_ << 13;
  random_ ^= random_ >> 17;
  random_ ^= random_ << 5;
  const float white = static_cast<float>(random_ >> 8) / 8388608.0f - 1.0f;
  pink_[0] = 0.99886f * pink_[0] + white * 0.0555179f;
  pink_[1] = 0.99332f * pink_[1] + white * 0.0750759f;
  pink_[2] = 0.96900f * pink_[2] + white * 0.1538520f;
  pink_[3] = 0.86650f * pink_[3] + white * 0.3104856f;
  pink_[4] = 0.55000f * pink_[4] + white * 0.5329522f;
  pink_[5] = -0.7616f * pink_[5] - white * 0.0168980f;
  const float sum = pink_[0] + pink_[1] + pink_[2] + pink_[3] + pink_[4] + pink_[5] + pink_[6] + white * 0.5362f;
  pink_[6] = white * 0.115926f;
  return std::clamp(sum * 0.11f * kPinkScale, -1.0f, 1.0f);
}

void TestTone::addTo(float* out, std::size_t frames, int channels) {
  if (out == nullptr || channels < 1) return;
  const std::uint32_t mask = mask_.load(std::memory_order_relaxed);
  if (mask == 0 && !active_) return;  // the usual case: nothing to do

  const std::size_t ch = static_cast<std::size_t>(channels);
  const std::size_t used = std::min(ch, static_cast<std::size_t>(kMaxChannels));
  const float target = amplitude_.load(std::memory_order_relaxed);
  bool sounding = false;
  for (std::size_t i = 0; i < frames; ++i) {
    amp_ += (target - amp_) * 0.002f;  // a level change glides instead of jumping
    const float v = next() * amp_;
    for (std::size_t c = 0; c < used; ++c) {
      const bool on = (mask & (std::uint32_t{1} << c)) != 0;
      float& fade = fade_[c];
      fade = on ? std::min(1.0f, fade + kFadeStep) : std::max(0.0f, fade - kFadeStep);
      if (fade > 0.0f) {
        out[i * ch + c] += v * fade;
        sounding = true;
      }
    }
  }
  // Channels the device does not have cannot be faded out here; forget them.
  for (std::size_t c = used; c < static_cast<std::size_t>(kMaxChannels); ++c) fade_[c] = 0.0f;
  active_ = sounding || mask != 0;
}

}  // namespace backhaul
