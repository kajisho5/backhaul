// The audio assignment table: which slot goes out on which output channel, at what level.
// Several slots may share one channel; they are summed.
#pragma once

#include <cstddef>
#include <vector>

namespace backhaul {

class RouteMatrix {
 public:
  RouteMatrix(std::size_t sources, std::size_t channels);

  std::size_t sources() const { return sources_; }
  std::size_t channels() const { return channels_; }

  // Out-of-range indexes are rejected (returns false, nothing changes).
  bool setRoute(std::size_t source, std::size_t channel, bool on, float gainDb = 0.0f);
  bool isOn(std::size_t source, std::size_t channel) const;

  // in:  sources() pointers, each to `frames` mono samples (a null pointer means silence).
  // out: channels() pointers, each to `frames` samples; overwritten.
  void mix(const float* const* in, float* const* out, std::size_t frames) const;

  static float dbToLinear(float db);

 private:
  std::size_t sources_, channels_;
  std::vector<float> gain_;  // linear, 0 = off; index = source * channels_ + channel
};

}  // namespace backhaul
