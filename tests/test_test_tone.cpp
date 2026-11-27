#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "check.h"
#include "core/audio_mixer.h"
#include "core/test_tone.h"

using backhaul::TestTone;

namespace {

float peakOf(const std::vector<float>& block, std::size_t channels, std::size_t channel, std::size_t from = 0) {
  float peak = 0.0f;
  for (std::size_t i = from; i * channels + channel < block.size(); ++i) peak = std::max(peak, std::fabs(block[i * channels + channel]));
  return peak;
}

float largestStep(const std::vector<float>& block, std::size_t channels, std::size_t channel) {
  float step = 0.0f;
  for (std::size_t i = 1; i * channels + channel < block.size(); ++i) {
    step = std::max(step, std::fabs(block[i * channels + channel] - block[(i - 1) * channels + channel]));
  }
  return step;
}

}  // namespace

void testTestTone() {
  const float minus20 = 0.1f;

  {  // silent until a channel is switched on, and then only on that channel
    TestTone tone;
    std::vector<float> block(4800 * 4, 0.0f);
    tone.addTo(block.data(), 4800, 4);
    CHECK_NEAR(peakOf(block, 4, 0), 0.0, 1e-12);

    tone.setChannels(0b0100);  // output 3
    CHECK(tone.channels() == 0b0100u);
    tone.addTo(block.data(), 4800, 4);
    CHECK_NEAR(peakOf(block, 4, 2, 2400), minus20, 0.002);  // -20 dB unless told otherwise
    CHECK_NEAR(peakOf(block, 4, 0), 0.0, 1e-12);
    CHECK_NEAR(peakOf(block, 4, 1), 0.0, 1e-12);
    CHECK_NEAR(peakOf(block, 4, 3), 0.0, 1e-12);

    // 1 kHz: the sign changes 2000 times a second.
    int crossings = 0;
    for (std::size_t i = 2401; i < 4800; ++i) {
      if ((block[i * 4 + 2] >= 0.0f) != (block[(i - 1) * 4 + 2] >= 0.0f)) ++crossings;
    }
    CHECK(crossings >= 99 && crossings <= 101);
  }

  {  // switching on and off fades: no jump larger than the sine's own steps
    TestTone tone;
    tone.setChannels(1);
    std::vector<float> on(960, 0.0f);
    tone.addTo(on.data(), 960, 1);
    CHECK(std::fabs(on[0]) < 0.001f);
    const float sineStep = minus20 * 2.0f * 3.14159265f * 1000.0f / 48000.0f;
    CHECK(largestStep(on, 1, 0) < sineStep * 1.5f);

    tone.setChannels(0);
    std::vector<float> off(960, 0.0f);
    tone.addTo(off.data(), 960, 1);
    CHECK(largestStep(off, 1, 0) < sineStep * 1.5f);
    CHECK(std::fabs(off[0] - on[959]) < sineStep * 1.5f);
    CHECK_NEAR(peakOf(off, 1, 0, 480), 0.0, 1e-12);  // gone after 5 ms
    std::vector<float> after(960, 0.0f);
    tone.addTo(after.data(), 960, 1);
    CHECK_NEAR(peakOf(after, 1, 0), 0.0, 1e-12);
  }

  {  // the level is limited at both ends
    TestTone tone;
    tone.setChannels(1);
    std::vector<float> block(48000, 0.0f);
    tone.setLevelDb(0.0f);  // asked for full level: stays at -6 dB
    tone.addTo(block.data(), block.size(), 1);
    CHECK_NEAR(peakOf(block, 1, 0, 24000), std::pow(10.0f, -6.0f / 20.0f), 0.005);
    tone.setLevelDb(-90.0f);
    std::fill(block.begin(), block.end(), 0.0f);
    tone.addTo(block.data(), block.size(), 1);
    CHECK_NEAR(peakOf(block, 1, 0, 24000), 0.01, 0.001);  // -40 dB
  }

  {  // pink noise: never above the level that was set, and far from silent
    TestTone tone;
    tone.setKind(TestTone::Kind::Pink);
    tone.setChannels(0b11);
    std::vector<float> block(48000 * 2 * 2, 0.0f);
    tone.addTo(block.data(), 48000 * 2, 2);
    CHECK(peakOf(block, 2, 0, 48000) <= minus20 * 1.001f);
    double sum = 0.0;
    for (std::size_t i = 48000; i < 96000; ++i) sum += static_cast<double>(block[i * 2]) * block[i * 2];
    const double rms = std::sqrt(sum / 48000.0);
    CHECK(rms > 0.015 && rms < 0.035);  // about a quarter of the peak
    CHECK(block[60000 * 2] == block[60000 * 2 + 1]);  // the same signal on every channel
    // More low than high: neighbouring samples are alike (white noise would be near 0).
    double alike = 0.0;
    for (std::size_t i = 48001; i < 96000; ++i) alike += static_cast<double>(block[i * 2]) * block[(i - 1) * 2];
    CHECK(alike / sum > 0.5);
  }

  {  // a device with fewer channels than the switched-on ones is no problem
    TestTone tone;
    tone.setChannels(0xffffffffu);
    std::vector<float> block(480 * 2, 0.0f);
    tone.addTo(block.data(), 480, 2);
    CHECK(peakOf(block, 2, 1, 240) > 0.05f);
    tone.addTo(nullptr, 480, 2);
    tone.addTo(block.data(), 480, 0);
  }

  {  // through the mixer: on top of the slots, and seen by the output meters
    backhaul::AudioMixer mixer(2);
    mixer.tone().setChannels(0b10);
    std::vector<float> block(4800 * 2, 1.0f);
    mixer.render(block.data(), 4800, 2);
    CHECK_NEAR(peakOf(block, 2, 0), 0.0, 1e-12);
    CHECK_NEAR(peakOf(block, 2, 1, 2400), minus20, 0.002);
    CHECK_NEAR(mixer.takeOutputPeak(0), 0.0, 1e-12);
    CHECK_NEAR(mixer.takeOutputPeak(1), minus20, 0.002);
  }
}
