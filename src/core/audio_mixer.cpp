#include "core/audio_mixer.h"

#include <algorithm>
#include <cmath>

namespace backhaul {
namespace {

constexpr std::size_t kRing = 1u << 16;            // 65536 samples, about 1.4 s
constexpr std::size_t kStartFill = AudioMixer::kRate / 25;   // 40 ms before a source starts playing
constexpr std::size_t kMaxLag = AudioMixer::kRate / 4;       // more than 250 ms waiting: catch up
constexpr std::size_t kCatchUpTo = AudioMixer::kRate / 20;   // ...down to 50 ms

}  // namespace

struct AudioMixer::Source {
  // Single-producer, single-consumer ring.
  std::vector<float> ring = std::vector<float>(kRing);
  std::atomic<std::size_t> write{0};
  std::atomic<std::size_t> read{0};
  std::atomic<bool> flushRequested{false};

  std::atomic<float> gain{1.0f};
  std::atomic<int> delaySamples{0};
  std::atomic<std::uint32_t> channels{0};
  std::atomic<float> peak{0.0f};

  // Delivery thread only.
  Resampler from32k{3, 2};
  std::vector<float> convert;
  std::vector<float> mono;

  // Device thread only.
  DelayLine delay{static_cast<std::size_t>(AudioMixer::kRate) * AudioMixer::kMaxDelayMs / 1000};
  int appliedDelay = 0;
  bool playing = false;

  std::size_t available() const { return write.load(std::memory_order_acquire) - read.load(std::memory_order_relaxed); }
};

AudioMixer::AudioMixer(int sources) {
  for (int i = 0; i < std::max(sources, 0); ++i) sources_.push_back(std::make_unique<Source>());
  for (auto& peak : outputPeak_) peak.store(0.0f);
}

AudioMixer::~AudioMixer() = default;

bool AudioMixer::push(int source, const std::int16_t* samples, std::size_t frames, int sampleRate, int channels) {
  if (source < 0 || source >= sources() || samples == nullptr || channels < 1 || channels > 2) return false;
  if (sampleRate != kRate && sampleRate != 32000) return false;
  Source& s = *sources_[static_cast<std::size_t>(source)];

  s.mono.resize(frames);
  float peak = 0.0f;
  for (std::size_t i = 0; i < frames; ++i) {
    const float v = static_cast<float>(samples[i * static_cast<std::size_t>(channels)]) / 32768.0f;
    s.mono[i] = v;
    peak = std::max(peak, std::fabs(v));
  }
  if (peak > s.peak.load(std::memory_order_relaxed)) s.peak.store(peak, std::memory_order_relaxed);

  const float* data = s.mono.data();
  std::size_t count = frames;
  if (sampleRate == 32000) {
    s.convert.clear();
    s.from32k.process(s.mono.data(), frames, s.convert);
    data = s.convert.data();
    count = s.convert.size();
  }
  // If the device is not taking audio (or far behind), drop the newest rather than block.
  const std::size_t w = s.write.load(std::memory_order_relaxed);
  const std::size_t r = s.read.load(std::memory_order_acquire);
  const std::size_t space = kRing - (w - r);
  count = std::min(count, space);
  for (std::size_t i = 0; i < count; ++i) s.ring[(w + i) & (kRing - 1)] = data[i];
  s.write.store(w + count, std::memory_order_release);
  return true;
}

void AudioMixer::flush(int source) {
  if (source < 0 || source >= sources()) return;
  sources_[static_cast<std::size_t>(source)]->flushRequested.store(true, std::memory_order_release);
}

void AudioMixer::setGainDb(int source, float db) {
  if (source < 0 || source >= sources()) return;
  sources_[static_cast<std::size_t>(source)]->gain.store(std::pow(10.0f, db / 20.0f), std::memory_order_relaxed);
}

void AudioMixer::setDelayMs(int source, int ms) {
  if (source < 0 || source >= sources()) return;
  const int clamped = std::clamp(ms, 0, kMaxDelayMs);
  sources_[static_cast<std::size_t>(source)]->delaySamples.store(clamped * (kRate / 1000), std::memory_order_relaxed);
}

void AudioMixer::setChannels(int source, std::uint32_t mask) {
  if (source < 0 || source >= sources()) return;
  sources_[static_cast<std::size_t>(source)]->channels.store(mask, std::memory_order_relaxed);
}

float AudioMixer::takePeak(int source) {
  if (source < 0 || source >= sources()) return 0.0f;
  return sources_[static_cast<std::size_t>(source)]->peak.exchange(0.0f, std::memory_order_relaxed);
}

void AudioMixer::render(float* out, std::size_t frames, int channels) {
  if (out == nullptr || channels < 1) return;
  const std::size_t ch = static_cast<std::size_t>(channels);
  std::fill(out, out + frames * ch, 0.0f);
  scratch_.resize(frames);

  for (const auto& holder : sources_) {
    Source& s = *holder;
    std::size_t r = s.read.load(std::memory_order_relaxed);
    if (s.flushRequested.exchange(false, std::memory_order_acquire)) {
      r = s.write.load(std::memory_order_acquire);
      s.read.store(r, std::memory_order_release);
      s.delay.clear();
      s.playing = false;
    }
    std::size_t have = s.write.load(std::memory_order_acquire) - r;
    if (have > kMaxLag) {  // the sender's clock runs faster than the device's: skip ahead
      r += have - kCatchUpTo;
      have = kCatchUpTo;
    }
    if (!s.playing && have >= kStartFill) s.playing = true;

    std::size_t take = 0;
    if (s.playing) {
      take = std::min(have, frames);
      for (std::size_t i = 0; i < take; ++i) scratch_[i] = s.ring[(r + i) & (kRing - 1)];
      if (take < frames) s.playing = false;  // ran dry: wait for a small reserve again
    }
    std::fill(scratch_.begin() + static_cast<std::ptrdiff_t>(take), scratch_.end(), 0.0f);
    s.read.store(r + take, std::memory_order_release);

    const int wanted = s.delaySamples.load(std::memory_order_relaxed);
    if (wanted != s.appliedDelay) {
      s.delay.setDelay(static_cast<std::size_t>(wanted));
      s.appliedDelay = wanted;
    }
    s.delay.process(scratch_.data(), scratch_.data(), frames);

    const std::uint32_t mask = s.channels.load(std::memory_order_relaxed);
    const float gain = s.gain.load(std::memory_order_relaxed);
    if (mask == 0 || gain == 0.0f) continue;
    for (std::size_t c = 0; c < ch && c < static_cast<std::size_t>(kMaxChannels); ++c) {
      if ((mask & (std::uint32_t{1} << c)) == 0) continue;
      for (std::size_t i = 0; i < frames; ++i) out[i * ch + c] += scratch_[i] * gain;
    }
  }

  for (std::size_t c = 0; c < ch && c < static_cast<std::size_t>(kMaxChannels); ++c) {
    float peak = 0.0f;
    for (std::size_t i = 0; i < frames; ++i) peak = std::max(peak, std::fabs(out[i * ch + c]));
    peak = std::min(peak, 1.0f);
    if (peak > outputPeak_[c].load(std::memory_order_relaxed)) outputPeak_[c].store(peak, std::memory_order_relaxed);
  }
}

float AudioMixer::takeOutputPeak(int channel) {
  if (channel < 0 || channel >= kMaxChannels) return 0.0f;
  return outputPeak_[channel].exchange(0.0f, std::memory_order_relaxed);
}

}  // namespace backhaul
