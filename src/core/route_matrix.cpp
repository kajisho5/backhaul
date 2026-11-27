#include "core/route_matrix.h"

#include <algorithm>
#include <cmath>

namespace backhaul {

RouteMatrix::RouteMatrix(std::size_t sources, std::size_t channels)
    : sources_(sources), channels_(channels), gain_(sources * channels, 0.0f) {}

float RouteMatrix::dbToLinear(float db) { return std::pow(10.0f, db / 20.0f); }

bool RouteMatrix::setRoute(std::size_t source, std::size_t channel, bool on, float gainDb) {
  if (source >= sources_ || channel >= channels_) return false;
  gain_[source * channels_ + channel] = on ? dbToLinear(gainDb) : 0.0f;
  return true;
}

bool RouteMatrix::isOn(std::size_t source, std::size_t channel) const {
  if (source >= sources_ || channel >= channels_) return false;
  return gain_[source * channels_ + channel] != 0.0f;
}

void RouteMatrix::mix(const float* const* in, float* const* out, std::size_t frames) const {
  for (std::size_t c = 0; c < channels_; ++c) std::fill(out[c], out[c] + frames, 0.0f);
  for (std::size_t s = 0; s < sources_; ++s) {
    if (in[s] == nullptr) continue;
    for (std::size_t c = 0; c < channels_; ++c) {
      const float g = gain_[s * channels_ + c];
      if (g == 0.0f) continue;
      float* o = out[c];
      const float* x = in[s];
      for (std::size_t i = 0; i < frames; ++i) o[i] += g * x[i];
    }
  }
}

}  // namespace backhaul
