// Fixed delay for one mono audio stream (lip-sync adjustment per slot).
#pragma once

#include <cstddef>
#include <vector>

namespace backhaul {

class DelayLine {
 public:
  explicit DelayLine(std::size_t maxDelaySamples);

  // Clamped to the maximum given at construction. Takes effect immediately;
  // already-stored audio is kept, so shortening skips and lengthening repeats nothing
  // but silence that was never written.
  void setDelay(std::size_t samples);
  std::size_t delay() const { return delay_; }
  std::size_t maxDelay() const { return buf_.size() - 1; }

  // in and out may be the same buffer.
  void process(const float* in, float* out, std::size_t frames);
  void clear();

 private:
  std::vector<float> buf_;
  std::size_t write_ = 0;
  std::size_t delay_ = 0;
};

}  // namespace backhaul
