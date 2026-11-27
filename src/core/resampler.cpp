#include "core/resampler.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace backhaul {
namespace {

constexpr double kPi = 3.14159265358979323846;

// Zeroth-order modified Bessel function of the first kind (for the Kaiser window).
double besselI0(double x) {
  double sum = 1.0, term = 1.0;
  for (int k = 1; k < 50; ++k) {
    term *= (x / (2.0 * k)) * (x / (2.0 * k));
    sum += term;
    if (term < 1e-12 * sum) break;
  }
  return sum;
}

}  // namespace

Resampler::Resampler(int up, int down, int tapsPerPhase) : up_(up), down_(down), taps_(tapsPerPhase) {
  if (up < 1 || down < 1 || tapsPerPhase < 2) throw std::invalid_argument("Resampler: bad ratio or taps");

  const int n = up_ * taps_;
  h_.resize(static_cast<std::size_t>(n));
  // Cut-off just below the lower of the two Nyquist limits, in cycles per upsampled sample.
  const double cutoff = 0.5 / std::max(up_, down_) * 0.92;
  const double beta = 9.0;
  const double centre = (n - 1) / 2.0;
  const double i0Beta = besselI0(beta);
  for (int i = 0; i < n; ++i) {
    const double t = i - centre;
    const double sinc = (t == 0.0) ? 2.0 * cutoff : std::sin(2.0 * kPi * cutoff * t) / (kPi * t);
    const double r = t / centre;
    const double window = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / i0Beta;
    h_[static_cast<std::size_t>(i)] = static_cast<float>(up_ * sinc * window);
  }
}

void Resampler::reset() {
  buf_.clear();
  bufStart_ = 0;
  inCount_ = 0;
  nextUp_ = 0;
}

double Resampler::delayInOutputSamples() const { return (up_ * taps_ - 1) / 2.0 / down_; }

void Resampler::process(const float* in, std::size_t frames, std::vector<float>& out) {
  buf_.insert(buf_.end(), in, in + frames);
  inCount_ += frames;

  const auto up = static_cast<std::uint64_t>(up_);
  const auto down = static_cast<std::uint64_t>(down_);
  const auto taps = static_cast<std::uint64_t>(taps_);

  // Output sample at upsampled index k uses inputs base, base-1, ... base-(taps-1), base = k / up.
  while (nextUp_ / up < inCount_) {
    const std::uint64_t base = nextUp_ / up;
    const std::uint64_t phase = nextUp_ % up;
    double acc = 0.0;
    for (std::uint64_t t = 0; t < taps; ++t) {
      if (t > base) break;  // before the start of the stream: silence
      const std::uint64_t idx = base - t;
      acc += static_cast<double>(h_[static_cast<std::size_t>(phase + t * up)]) *
             static_cast<double>(buf_[static_cast<std::size_t>(idx - bufStart_)]);
    }
    out.push_back(static_cast<float>(acc));
    nextUp_ += down;
  }

  // Drop inputs no future output can reach.
  const std::uint64_t nextBase = nextUp_ / up;
  const std::uint64_t keepFrom = (nextBase >= taps - 1) ? nextBase - (taps - 1) : 0;
  if (keepFrom > bufStart_) {
    const std::uint64_t drop = std::min<std::uint64_t>(keepFrom - bufStart_, buf_.size());
    buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(drop));
    bufStart_ += drop;
  }
}

}  // namespace backhaul
