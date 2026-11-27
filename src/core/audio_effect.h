// Something that changes sound in place, such as a plug-in. process() runs on a sound
// thread and must not wait for anything; everything else is called from another thread.
#pragma once

#include <cstddef>

namespace backhaul {

class AudioEffect {
 public:
  virtual ~AudioEffect() = default;
  // `channels[c]` points to `frames` samples of channel c (48 kHz). Changed in place.
  virtual void process(float* const* channels, int channelCount, std::size_t frames) = 0;
  // How much later the sound comes out, in samples.
  virtual int latencySamples() const = 0;
};

}  // namespace backhaul
