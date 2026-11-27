// A short row of effects that one stream of sound goes through, one after the other.
// The row is changed from the settings thread while a sound thread keeps processing:
// during the instant of a change the sound passes through unchanged instead of waiting.
#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

#include "core/audio_effect.h"

namespace backhaul {

class EffectChain {
 public:
  static constexpr int kMaxEffects = 4;

  // Settings thread. Replaces the whole row (at most kMaxEffects are kept; nulls are dropped).
  // Effects that are no longer in the row are let go on this thread, never on the sound thread.
  void set(std::vector<std::shared_ptr<AudioEffect>> effects);
  // A bypassed effect stays in the row but the sound goes around it.
  void setBypassed(int index, bool bypassed);
  bool bypassed(int index) const;
  int size() const { return count_.load(std::memory_order_relaxed); }
  // Delay of everything that is not bypassed, in samples.
  int latencySamples() const;

  // Sound thread.
  void process(float* const* channels, int channelCount, std::size_t frames);

 private:
  mutable std::mutex lock_;  // the sound thread only ever tries it
  std::vector<std::shared_ptr<AudioEffect>> effects_;
  std::atomic<bool> bypass_[kMaxEffects] = {};
  std::atomic<int> count_{0};
};

}  // namespace backhaul
