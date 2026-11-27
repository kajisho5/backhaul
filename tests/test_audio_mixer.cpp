#include <cmath>
#include <cstdint>
#include <vector>

#include "check.h"
#include "core/audio_mixer.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

std::vector<std::int16_t> tone(int rate, double hz, std::size_t frames, double amplitude, std::size_t start = 0) {
  std::vector<std::int16_t> v(frames);
  for (std::size_t i = 0; i < frames; ++i) {
    v[i] = static_cast<std::int16_t>(std::lround(amplitude * 32767.0 * std::sin(2.0 * kPi * hz * static_cast<double>(start + i) / rate)));
  }
  return v;
}

double rms(const std::vector<float>& interleaved, int channels, int channel, std::size_t from, std::size_t to) {
  double sum = 0.0;
  for (std::size_t i = from; i < to; ++i) {
    const double v = interleaved[i * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channel)];
    sum += v * v;
  }
  return std::sqrt(sum / static_cast<double>(to - from));
}

}  // namespace

void testAudioMixer() {
  using backhaul::AudioMixer;
  const int ch = 4;

  {  // routing, level, and silence everywhere else
    AudioMixer m(3);
    m.setChannels(0, 0b0001);
    m.setChannels(1, 0b0101);  // source 1 goes to channels 1 and 3
    m.setGainDb(1, -6.0f);
    const auto a = tone(48000, 440.0, 9600, 0.5);
    CHECK(m.push(0, a.data(), a.size(), 48000));
    CHECK(m.push(1, a.data(), a.size(), 48000));
    CHECK(m.push(2, a.data(), a.size(), 48000));  // source 2 is assigned nowhere
    std::vector<float> out(4800 * ch, 9.0f);
    m.render(out.data(), 4800, ch);
    const double full = 0.5 / std::sqrt(2.0);
    const double half = full * std::pow(10.0, -6.0 / 20.0);
    CHECK_NEAR(rms(out, ch, 0, 0, 4800), full + half, 0.004);  // both sources, in phase
    CHECK_NEAR(rms(out, ch, 1, 0, 4800), 0.0, 1e-9);
    CHECK_NEAR(rms(out, ch, 2, 0, 4800), half, 0.003);
    CHECK_NEAR(rms(out, ch, 3, 0, 4800), 0.0, 1e-9);
    CHECK_NEAR(m.takePeak(0), 0.5, 0.001);
    CHECK_NEAR(m.takePeak(0), 0.0, 1e-9);  // read once, then reset
  }

  {  // nothing delivered: silence, and a late start waits for a small reserve
    AudioMixer m(1);
    m.setChannels(0, 1);
    std::vector<float> out(480 * ch, 9.0f);
    m.render(out.data(), 480, ch);
    CHECK_NEAR(rms(out, ch, 0, 0, 480), 0.0, 1e-9);
    const auto a = tone(48000, 440.0, 480, 0.5);  // 10 ms: below the 40 ms reserve
    m.push(0, a.data(), a.size(), 48000);
    m.render(out.data(), 480, ch);
    CHECK_NEAR(rms(out, ch, 0, 0, 480), 0.0, 1e-9);
    for (int i = 0; i < 4; ++i) m.push(0, a.data(), a.size(), 48000);  // now 50 ms
    m.render(out.data(), 480, ch);
    CHECK(rms(out, ch, 0, 0, 480) > 0.3);
  }

  {  // delay: the same audio appears later by exactly the set time
    AudioMixer m(1);
    m.setChannels(0, 1);
    m.setDelayMs(0, 100);
    const auto a = tone(48000, 440.0, 9600, 0.5);
    m.push(0, a.data(), a.size(), 48000);
    std::vector<float> out(9600 * ch);
    m.render(out.data(), 9600, ch);
    CHECK_NEAR(rms(out, ch, 0, 0, 4800), 0.0, 1e-9);            // first 100 ms silent
    CHECK(rms(out, ch, 0, 4800, 9600) > 0.3);
    CHECK_NEAR(out[4800 * ch], 0.0, 1e-4);                      // the tone starts at its first sample
    CHECK_NEAR(out[(4800 + 27) * ch], a[27] / 32768.0, 1e-4);
  }

  {  // 32 kHz in, 48 kHz out: 1.5 times as many samples, same pitch and level
    AudioMixer m(1);
    m.setChannels(0, 1);
    std::vector<float> out;
    std::vector<float> block(480 * ch);
    for (int i = 0; i < 100; ++i) {  // one second, in 10 ms pieces as Zoom delivers it
      const auto a = tone(32000, 1000.0, 320, 0.5, static_cast<std::size_t>(i) * 320);
      CHECK(m.push(0, a.data(), a.size(), 32000));
      m.render(block.data(), 480, ch);
      out.insert(out.end(), block.begin(), block.end());
    }
    const std::size_t frames = out.size() / ch;
    CHECK_NEAR(rms(out, ch, 0, frames / 2, frames), 0.5 / std::sqrt(2.0), 0.01);
    int crossings = 0;  // upward zero crossings in the last half second: 500 for 1 kHz
    for (std::size_t i = frames / 2 + 1; i < frames; ++i) {
      if (out[(i - 1) * ch] < 0.0f && out[i * ch] >= 0.0f) ++crossings;
    }
    CHECK(crossings >= 498 && crossings <= 502);
  }

  {  // stereo input uses the left channel; odd formats and bad indexes are refused
    AudioMixer m(1);
    m.setChannels(0, 1);
    std::vector<std::int16_t> stereo(9600 * 2);
    for (std::size_t i = 0; i < 9600; ++i) {
      stereo[i * 2] = 8000;
      stereo[i * 2 + 1] = -30000;
    }
    CHECK(m.push(0, stereo.data(), 9600, 48000, 2));
    std::vector<float> out(4800 * ch);
    m.render(out.data(), 4800, ch);
    CHECK_NEAR(out[100 * ch], 8000.0 / 32768.0, 1e-4);
    CHECK(!m.push(0, stereo.data(), 100, 44100));
    CHECK(!m.push(0, stereo.data(), 100, 48000, 3));
    CHECK(!m.push(5, stereo.data(), 100, 48000));
    CHECK(!m.push(-1, stereo.data(), 100, 48000));
    CHECK(!m.push(0, nullptr, 100, 48000));
    m.setGainDb(9, 0.0f);
    m.setDelayMs(-3, 10);
    m.setChannels(7, 1);
    CHECK_NEAR(m.takePeak(4), 0.0, 1e-9);
  }

  {  // a sender that runs ahead does not build up delay without limit
    AudioMixer m(1);
    m.setChannels(0, 1);
    const auto a = tone(48000, 440.0, 4800, 0.5);
    for (int i = 0; i < 8; ++i) m.push(0, a.data(), a.size(), 48000);  // 800 ms waiting
    std::vector<float> out(480 * ch);
    m.render(out.data(), 480, ch);
    int blocks = 1;  // how long until it runs dry, in 10 ms blocks
    while (blocks < 200) {
      m.render(out.data(), 480, ch);
      if (rms(out, ch, 0, 0, 480) < 1e-6) break;
      ++blocks;
    }
    CHECK(blocks <= 6);  // about 50 ms were kept, not 800
  }

  {  // flush forgets what was waiting
    AudioMixer m(1);
    m.setChannels(0, 1);
    const auto a = tone(48000, 440.0, 9600, 0.5);
    m.push(0, a.data(), a.size(), 48000);
    m.flush(0);
    std::vector<float> out(480 * ch);
    m.render(out.data(), 480, ch);
    CHECK_NEAR(rms(out, ch, 0, 0, 480), 0.0, 1e-9);
  }

  {  // more device channels than the mask can name, and a mono device
    AudioMixer m(1);
    m.setChannels(0, 0xffffffffu);
    const auto a = tone(48000, 440.0, 9600, 0.5);
    m.push(0, a.data(), a.size(), 48000);
    std::vector<float> wide(480 * 40);
    m.render(wide.data(), 480, 40);
    CHECK(rms(wide, 40, 31, 0, 480) > 0.3);
    CHECK_NEAR(rms(wide, 40, 32, 0, 480), 0.0, 1e-9);
    std::vector<float> mono(480);
    m.render(mono.data(), 480, 1);
    CHECK(rms(mono, 1, 0, 0, 480) > 0.3);
  }

  {  // output meters: the loudest sample per output channel since last asked
    AudioMixer m(2);
    m.setChannels(0, 0b01);
    m.setChannels(1, 0b10);
    m.setGainDb(1, -6.0f);
    const auto a = tone(48000, 440.0, 9600, 0.5);
    m.push(0, a.data(), a.size(), 48000);
    m.push(1, a.data(), a.size(), 48000);
    std::vector<float> out(4800 * 3);
    m.render(out.data(), 4800, 3);
    CHECK_NEAR(m.takeOutputPeak(0), 0.5, 0.01);
    CHECK_NEAR(m.takeOutputPeak(1), 0.25, 0.01);
    CHECK(m.takeOutputPeak(2) == 0.0f);   // nothing is sent there
    CHECK(m.takeOutputPeak(0) == 0.0f);   // asked already
    CHECK(m.takeOutputPeak(-1) == 0.0f && m.takeOutputPeak(AudioMixer::kMaxChannels) == 0.0f);
  }
}
