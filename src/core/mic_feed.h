// Sound on its way from a device to the meeting. The device's thread writes what it
// captured; another thread takes it out in small 16-bit blocks to send. Neither waits
// for the other. Everything is 48 kHz.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace backhaul {

class MicFeed {
 public:
  static constexpr int kRate = 48000;

  // Holds up to `capacityFrames` (rounded up to a power of two) before new sound is dropped.
  explicit MicFeed(std::size_t capacityFrames = 48000);

  // Device thread. `interleaved` has `channels` samples per frame; the pair starting at
  // `firstChannel` (0-based) is kept. A device with a single channel there gives the same
  // sound on both sides. Out-of-range requests are ignored.
  void write(const float* interleaved, std::size_t frames, int channels, int firstChannel = 0);

  // Sending thread. Takes up to `maxFrames`; stereo gives left/right interleaved, mono the
  // average of both. Returns the number of frames written to `out`.
  std::size_t read(std::int16_t* out, std::size_t maxFrames, bool stereo);
  // Frames waiting to be read.
  std::size_t available() const;
  // Forget what is waiting (sending thread).
  void clear();

  // Any thread: loudest sample since the last call, 0..1. side 0 = left, 1 = right.
  float takePeak(int side);

 private:
  std::vector<float> ring_;  // left, right, left, right, ...
  std::size_t mask_ = 0;     // in frames
  std::atomic<std::size_t> write_{0}, read_{0};
  std::atomic<float> peak_[2];
};

}  // namespace backhaul
