#include "core/effect_chain.h"

#include <utility>

namespace backhaul {

void EffectChain::set(std::vector<std::shared_ptr<AudioEffect>> effects) {
  std::vector<std::shared_ptr<AudioEffect>> next;
  for (auto& effect : effects) {
    if (effect != nullptr && next.size() < static_cast<std::size_t>(kMaxEffects)) next.push_back(std::move(effect));
  }
  {
    const std::lock_guard<std::mutex> hold(lock_);
    effects_.swap(next);
    count_.store(static_cast<int>(effects_.size()), std::memory_order_relaxed);
  }
  // `next` now holds the old row and lets it go here, outside the lock.
}

void EffectChain::setBypassed(int index, bool bypassed) {
  if (index < 0 || index >= kMaxEffects) return;
  bypass_[index].store(bypassed, std::memory_order_relaxed);
}

bool EffectChain::bypassed(int index) const {
  return index >= 0 && index < kMaxEffects && bypass_[index].load(std::memory_order_relaxed);
}

int EffectChain::latencySamples() const {
  const std::lock_guard<std::mutex> hold(lock_);
  int total = 0;
  for (std::size_t i = 0; i < effects_.size(); ++i) {
    if (!bypass_[i].load(std::memory_order_relaxed)) total += effects_[i]->latencySamples();
  }
  return total;
}

void EffectChain::process(float* const* channels, int channelCount, std::size_t frames) {
  if (count_.load(std::memory_order_relaxed) == 0 || channels == nullptr || channelCount < 1 || frames == 0) return;
  const std::unique_lock<std::mutex> hold(lock_, std::try_to_lock);
  if (!hold.owns_lock()) return;  // the row is being changed right now
  for (std::size_t i = 0; i < effects_.size(); ++i) {
    if (!bypass_[i].load(std::memory_order_relaxed)) effects_[i]->process(channels, channelCount, frames);
  }
}

}  // namespace backhaul
