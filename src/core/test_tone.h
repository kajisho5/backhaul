// A sound-check signal for the output channels: a 1 kHz sine or pink noise, switched on
// per channel. Settings come from one thread, addTo() runs on the sound device's thread;
// neither waits for the other.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace backhaul {

class TestTone {
 public:
  enum class Kind { Sine, Pink };
  static constexpr int kRate = 48000;
  static constexpr int kMaxChannels = 32;
  // The level is the loudest sample of the signal (what a peak meter reads). It cannot be
  // set near full level: a slip of the finger must not hurt speakers or ears.
  static constexpr float kMinDb = -40.0f;
  static constexpr float kMaxDb = -6.0f;
  static constexpr float kDefaultDb = -20.0f;

  // Settings thread.
  void setKind(Kind kind);
  void setLevelDb(float db);             // clamped to kMinDb..kMaxDb
  void setChannels(std::uint32_t mask);  // bit 0 = output channel 1; 0 = silent
  std::uint32_t channels() const { return mask_.load(std::memory_order_relaxed); }

  // Device thread. Adds the signal to `frames * channels` interleaved samples.
  // Channels fade in and out over a few milliseconds, so switching does not click.
  void addTo(float* out, std::size_t frames, int channels);

 private:
  float next();  // one sample at full scale (-1..1)

  std::atomic<std::uint32_t> mask_{0};
  std::atomic<float> amplitude_{0.1f};  // -20 dB
  std::atomic<int> kind_{0};

  // Device thread only.
  float fade_[kMaxChannels] = {};
  float amp_ = 0.1f;
  bool active_ = false;
  std::uint32_t phase_ = 0;          // sine: sample counter within one second
  std::uint32_t random_ = 0x1234abcdu;
  float pink_[7] = {};
};

}  // namespace backhaul
