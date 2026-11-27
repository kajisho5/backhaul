#include "core/delay_line.h"

#include <algorithm>

namespace backhaul {

DelayLine::DelayLine(std::size_t maxDelaySamples) : buf_(maxDelaySamples + 1, 0.0f) {}

void DelayLine::setDelay(std::size_t samples) { delay_ = std::min(samples, maxDelay()); }

void DelayLine::clear() { std::fill(buf_.begin(), buf_.end(), 0.0f); }

void DelayLine::process(const float* in, float* out, std::size_t frames) {
  const std::size_t n = buf_.size();
  for (std::size_t i = 0; i < frames; ++i) {
    buf_[write_] = in[i];
    const std::size_t read = (write_ + n - delay_) % n;
    out[i] = buf_[read];
    write_ = (write_ + 1) % n;
  }
}

}  // namespace backhaul
