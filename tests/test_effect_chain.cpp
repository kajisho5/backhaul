#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "check.h"
#include "core/audio_mixer.h"
#include "core/effect_chain.h"
#include "core/mic_feed.h"

using backhaul::AudioEffect;
using backhaul::EffectChain;

namespace {

// Multiplies and adds, so the order of two of them can be seen in the result.
struct Scale final : AudioEffect {
  float times, plus;
  int delay;
  std::atomic<int>* gone;
  Scale(float t, float p, int d = 0, std::atomic<int>* g = nullptr) : times(t), plus(p), delay(d), gone(g) {}
  ~Scale() override {
    if (gone != nullptr) ++*gone;
  }
  void process(float* const* channels, int channelCount, std::size_t frames) override {
    for (int c = 0; c < channelCount; ++c) {
      for (std::size_t i = 0; i < frames; ++i) channels[c][i] = channels[c][i] * times + plus;
    }
  }
  int latencySamples() const override { return delay; }
};

}  // namespace

void testEffectChain() {
  {  // empty: the sound is untouched
    EffectChain chain;
    float data[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    float* one = data;
    chain.process(&one, 1, 4);
    CHECK_NEAR(data[2], 0.3, 1e-6);
    CHECK(chain.size() == 0 && chain.latencySamples() == 0);
    chain.process(nullptr, 1, 4);
    chain.process(&one, 0, 4);
  }

  {  // in order, on every channel, with bypass and the sum of the delays
    EffectChain chain;
    chain.set({std::make_shared<Scale>(2.0f, 0.0f, 100), std::make_shared<Scale>(1.0f, 0.5f, 28)});
    float left[2] = {0.1f, 0.2f}, right[2] = {-0.1f, 0.0f};
    float* both[2] = {left, right};
    chain.process(both, 2, 2);
    CHECK_NEAR(left[1], 0.9, 1e-6);    // 0.2 * 2 + 0.5
    CHECK_NEAR(right[0], 0.3, 1e-6);
    CHECK(chain.size() == 2 && chain.latencySamples() == 128);

    chain.setBypassed(0, true);
    CHECK(chain.bypassed(0) && !chain.bypassed(1));
    float mono[1] = {0.2f};
    float* one = mono;
    chain.process(&one, 1, 1);
    CHECK_NEAR(mono[0], 0.7, 1e-6);
    CHECK(chain.latencySamples() == 28);
    chain.setBypassed(9, true);  // no such place
    chain.setBypassed(-1, true);
    CHECK(!chain.bypassed(9));
  }

  {  // at most four, nulls are dropped, and replaced effects are let go
    std::atomic<int> gone{0};
    EffectChain chain;
    std::vector<std::shared_ptr<AudioEffect>> many;
    many.push_back(nullptr);
    for (int i = 0; i < 6; ++i) many.push_back(std::make_shared<Scale>(1.0f, 1.0f, 0, &gone));
    chain.set(std::move(many));
    CHECK(chain.size() == 4 && gone == 2);
    float v[1] = {0.0f};
    float* one = v;
    chain.process(&one, 1, 1);
    CHECK_NEAR(v[0], 4.0, 1e-6);
    chain.set({});
    CHECK(chain.size() == 0 && gone == 6);
  }

  {  // changing the row while sound runs: every block is either untouched or fully processed
    EffectChain chain;
    std::atomic<bool> stop{false};
    std::atomic<int> wrong{0};
    std::thread sound([&] {
      std::vector<float> block(64);
      while (!stop) {
        for (float& v : block) v = 1.0f;
        float* one = block.data();
        chain.process(&one, 1, block.size());
        for (float v : block) {
          if (v != 1.0f && v != 3.0f) ++wrong;
        }
      }
    });
    for (int i = 0; i < 2000; ++i) {
      chain.set({std::make_shared<Scale>(3.0f, 0.0f)});
      chain.set({});
    }
    stop = true;
    sound.join();
    CHECK(wrong == 0);
  }

  {  // in the mixer: after the delay, before the level, only for that source
    backhaul::AudioMixer mixer(2);
    CHECK(mixer.effects(0) != nullptr && mixer.effects(2) == nullptr && mixer.effects(-1) == nullptr);
    mixer.effects(0)->set({std::make_shared<Scale>(0.5f, 0.0f)});
    mixer.setChannels(0, 0b01);
    mixer.setChannels(1, 0b10);
    mixer.setGainDb(0, 0.0f);
    const std::vector<std::int16_t> loud(9600, 16384);  // 0.5
    mixer.push(0, loud.data(), loud.size(), 48000);
    mixer.push(1, loud.data(), loud.size(), 48000);
    std::vector<float> out(4800 * 2);
    mixer.render(out.data(), 4800, 2);
    CHECK_NEAR(out[2000 * 2], 0.25, 1e-3);      // source 0 went through the effect
    CHECK_NEAR(out[2000 * 2 + 1], 0.5, 1e-3);   // source 1 did not
  }

  {  // the microphone feed as floats, and back to what the meeting takes
    backhaul::MicFeed feed;
    const float in[6] = {0.5f, -0.5f, 0.25f, 0.75f, 1.5f, -1.5f};
    feed.write(in, 3, 2);
    float left[4], right[4];
    CHECK(feed.readFloat(left, right, 4) == 3);
    CHECK_NEAR(left[1], 0.25, 1e-9);
    CHECK_NEAR(right[1], 0.75, 1e-9);
    CHECK(feed.available() == 0 && feed.readFloat(nullptr, right, 4) == 0);
    std::int16_t stereo[6], mono[3];
    backhaul::MicFeed::toPcm(left, right, 3, true, stereo);
    backhaul::MicFeed::toPcm(left, right, 3, false, mono);
    CHECK(stereo[0] == 16384 && stereo[1] == -16384);
    CHECK(stereo[4] == 32767 && stereo[5] == -32767);  // held at full level
    CHECK(mono[0] == 0 && mono[1] == 16384);
  }
}
