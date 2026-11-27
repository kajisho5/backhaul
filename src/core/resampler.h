// Streaming sample-rate converter by a fixed ratio up/down (for example 3/2 turns
// Zoom's 32 kHz audio into 48 kHz). Polyphase windowed-sinc filter.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace backhaul {

class Resampler {
 public:
  // up and down must be >= 1. tapsPerPhase sets quality (and delay); 32 is plenty for speech.
  Resampler(int up, int down, int tapsPerPhase = 32);

  // Appends the converted samples to `out`. Feeding the same audio in different
  // chunk sizes produces exactly the same output.
  void process(const float* in, std::size_t frames, std::vector<float>& out);

  void reset();

  // Delay through the converter, measured in output samples.
  double delayInOutputSamples() const;

 private:
  int up_, down_, taps_;
  std::vector<float> h_;         // prototype filter, length up_ * taps_
  std::vector<float> buf_;       // input samples still needed
  std::uint64_t bufStart_ = 0;   // absolute input index of buf_[0]
  std::uint64_t inCount_ = 0;    // absolute input samples received so far
  std::uint64_t nextUp_ = 0;     // absolute index, at the upsampled rate, of the next output
};

}  // namespace backhaul
