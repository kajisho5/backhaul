// Takes each slot's incoming speech and produces the output channels: level, delay,
// and the assignment of slots to channels. Built for three threads that never wait
// for each other: one that delivers audio per slot, one that changes settings, and
// the sound device's thread that calls render().
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/delay_line.h"
#include "core/resampler.h"

namespace backhaul {

class AudioMixer {
 public:
  static constexpr int kRate = 48000;       // what render() produces
  static constexpr int kMaxDelayMs = 2000;
  static constexpr int kMaxChannels = 32;

  explicit AudioMixer(int sources);
  ~AudioMixer();

  int sources() const { return static_cast<int>(sources_.size()); }

  // Delivery thread (one at a time per source). 16-bit samples; with two channels the
  // left one is used. Accepts 48 kHz and 32 kHz. Returns false for anything else.
  bool push(int source, const std::int16_t* samples, std::size_t frames, int sampleRate, int channels = 1);
  // Forget what is waiting for this source (the person left or was replaced).
  void flush(int source);

  // Settings thread.
  void setGainDb(int source, float db);
  void setDelayMs(int source, int ms);
  void setChannels(int source, std::uint32_t mask);  // bit 0 = output channel 1
  // Loudest sample since the last call, 0..1, before gain.
  float takePeak(int source);

  // Device thread. Writes frames * channels samples, channel-interleaved.
  void render(float* out, std::size_t frames, int channels);

 private:
  struct Source;
  std::vector<std::unique_ptr<Source>> sources_;
  std::vector<float> scratch_;
};

}  // namespace backhaul
