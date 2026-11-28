#include "core/mic_feed.h"

#include <algorithm>
#include <cmath>

namespace backhaul {

namespace {

std::size_t powerOfTwoAtLeast(std::size_t n) {
  std::size_t p = 64;
  while (p < n) p <<= 1;
  return p;
}

std::int16_t toInt16(float v) {
  if (std::isnan(v)) return 0;  // not a number (a plug-in gone wrong): silence, never a full-level click
  if (v < -1.0f) v = -1.0f;
  if (v > 1.0f) v = 1.0f;
  return static_cast<std::int16_t>(std::lround(v * 32767.0f));
}

void raise(std::atomic<float>& peak, float value) {
  float seen = peak.load(std::memory_order_relaxed);
  while (value > seen && !peak.compare_exchange_weak(seen, value, std::memory_order_relaxed)) {
  }
}

}  // namespace

MicFeed::MicFeed(std::size_t capacityFrames) {
  const std::size_t frames = powerOfTwoAtLeast(capacityFrames);
  ring_.assign(frames * 2, 0.0f);
  mask_ = frames - 1;
  peak_[0].store(0.0f);
  peak_[1].store(0.0f);
}

void MicFeed::write(const float* interleaved, std::size_t frames, int channels, int firstChannel) {
  if (interleaved == nullptr || channels < 1 || firstChannel < 0 || firstChannel >= channels) return;
  const std::size_t ch = static_cast<std::size_t>(channels);
  const std::size_t left = static_cast<std::size_t>(firstChannel);
  const std::size_t right = left + 1 < ch ? left + 1 : left;
  std::size_t w = write_.load(std::memory_order_relaxed);
  const std::size_t room = mask_ + 1 - (w - read_.load(std::memory_order_acquire));
  const std::size_t take = std::min(frames, room);  // when full, the newest sound is dropped
  float peakLeft = 0.0f, peakRight = 0.0f;
  for (std::size_t i = 0; i < frames; ++i) {
    const float l = interleaved[i * ch + left];
    const float r = interleaved[i * ch + right];
    const float al = std::fabs(l), ar = std::fabs(r);
    if (al > peakLeft) peakLeft = al;   // NaN never compares greater, so it is skipped
    if (ar > peakRight) peakRight = ar;
    if (i < take) {
      const std::size_t at = ((w + i) & mask_) * 2;
      ring_[at] = l;
      ring_[at + 1] = r;
    }
  }
  write_.store(w + take, std::memory_order_release);
  raise(peak_[0], std::min(peakLeft, 1.0f));
  raise(peak_[1], std::min(peakRight, 1.0f));
}

std::size_t MicFeed::available() const {
  return write_.load(std::memory_order_acquire) - read_.load(std::memory_order_acquire);
}

std::size_t MicFeed::read(std::int16_t* out, std::size_t maxFrames, bool stereo) {
  if (out == nullptr) return 0;
  const std::size_t r = read_.load(std::memory_order_relaxed);
  const std::size_t take = std::min(maxFrames, write_.load(std::memory_order_acquire) - r);
  for (std::size_t i = 0; i < take; ++i) {
    const std::size_t at = ((r + i) & mask_) * 2;
    if (stereo) {
      out[i * 2] = toInt16(ring_[at]);
      out[i * 2 + 1] = toInt16(ring_[at + 1]);
    } else {
      out[i] = toInt16((ring_[at] + ring_[at + 1]) * 0.5f);
    }
  }
  read_.store(r + take, std::memory_order_release);
  return take;
}

void MicFeed::clear() { read_.store(write_.load(std::memory_order_acquire), std::memory_order_release); }

float MicFeed::takePeak(int side) {
  if (side < 0 || side > 1) return 0.0f;
  return peak_[side].exchange(0.0f, std::memory_order_relaxed);
}

}  // namespace backhaul

namespace backhaul {

std::size_t MicFeed::readFloat(float* left, float* right, std::size_t maxFrames) {
  if (left == nullptr || right == nullptr) return 0;
  const std::size_t r = read_.load(std::memory_order_relaxed);
  const std::size_t take = std::min(maxFrames, write_.load(std::memory_order_acquire) - r);
  for (std::size_t i = 0; i < take; ++i) {
    const std::size_t at = ((r + i) & mask_) * 2;
    left[i] = ring_[at];
    right[i] = ring_[at + 1];
  }
  read_.store(r + take, std::memory_order_release);
  return take;
}

void MicFeed::toPcm(const float* left, const float* right, std::size_t frames, bool stereo, std::int16_t* out) {
  if (left == nullptr || right == nullptr || out == nullptr) return;
  for (std::size_t i = 0; i < frames; ++i) {
    if (stereo) {
      out[i * 2] = toInt16(left[i]);
      out[i * 2 + 1] = toInt16(right[i]);
    } else {
      out[i] = toInt16((left[i] + right[i]) * 0.5f);
    }
  }
}

}  // namespace backhaul
