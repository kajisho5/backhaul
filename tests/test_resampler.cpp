#include <algorithm>
#include <cmath>
#include <vector>

#include "check.h"
#include "core/resampler.h"

using namespace backhaul;

namespace {
constexpr double kPi = 3.14159265358979323846;

std::vector<float> sine(double freq, double rate, std::size_t n) {
  std::vector<float> v(n);
  for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<float>(std::sin(2.0 * kPi * freq * static_cast<double>(i) / rate));
  return v;
}

// Signal-to-error ratio in dB of the converted sine against the ideal 48 kHz sine,
// skipping the start-up part where the filter is still filling.
double snrDb(double freq) {
  Resampler r(3, 2);
  const std::vector<float> in = sine(freq, 32000.0, 32000);
  std::vector<float> out;
  r.process(in.data(), in.size(), out);
  const double delay = r.delayInOutputSamples();
  double sig = 0.0, err = 0.0;
  for (std::size_t m = 200; m < out.size(); ++m) {
    const double ideal = std::sin(2.0 * kPi * freq * (static_cast<double>(m) - delay) / 48000.0);
    sig += ideal * ideal;
    err += (out[m] - ideal) * (out[m] - ideal);
  }
  return 10.0 * std::log10(sig / err);
}
}  // namespace

void testResampler() {
  {  // 32 kHz -> 48 kHz makes exactly 1.5x as many samples
    Resampler r(3, 2);
    std::vector<float> in(32000, 0.0f), out;
    r.process(in.data(), in.size(), out);
    CHECK(out.size() == 48000);
  }
  // Speech-band tones come through clean.
  CHECK(snrDb(300.0) > 70.0);
  CHECK(snrDb(1000.0) > 70.0);
  CHECK(snrDb(4000.0) > 70.0);
  CHECK(snrDb(12000.0) > 60.0);

  {  // a steady level stays at the same level (no gain error)
    Resampler r(3, 2);
    std::vector<float> in(4000, 0.25f), out;
    r.process(in.data(), in.size(), out);
    CHECK_NEAR(out[out.size() / 2], 0.25, 1e-4);
    CHECK_NEAR(out.back(), 0.25, 1e-4);
  }
  {  // chunk size must not change the result: 10 ms blocks vs odd sizes vs all at once
    const std::vector<float> in = sine(440.0, 32000.0, 6400);
    Resampler whole(3, 2), blocks(3, 2), odd(3, 2);
    std::vector<float> a, b, c;
    whole.process(in.data(), in.size(), a);
    for (std::size_t i = 0; i < in.size(); i += 320) blocks.process(in.data() + i, 320, b);
    std::size_t i = 0, step = 1;
    while (i < in.size()) {
      const std::size_t n = std::min(step, in.size() - i);
      odd.process(in.data() + i, n, c);
      i += n;
      step = step * 3 % 97 + 1;
    }
    CHECK(a == b);
    CHECK(a == c);
  }
  {  // an empty block is harmless
    Resampler r(3, 2);
    std::vector<float> out;
    r.process(nullptr, 0, out);
    CHECK(out.empty());
  }
  {  // reset starts a fresh stream
    const std::vector<float> in = sine(440.0, 32000.0, 640);
    Resampler r(3, 2);
    std::vector<float> first, second;
    r.process(in.data(), in.size(), first);
    r.reset();
    r.process(in.data(), in.size(), second);
    CHECK(first == second);
  }
  {  // same rate in and out (1/1) is a plain low-pass with the stated delay
    Resampler r(1, 1);
    std::vector<float> in(2000, 1.0f), out;
    r.process(in.data(), in.size(), out);
    CHECK(out.size() == 2000);
    CHECK_NEAR(out.back(), 1.0, 1e-3);
  }
  {  // bad arguments are refused loudly
    bool threw = false;
    try { Resampler bad(0, 2); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
  }
}
