#include "core/nv12.h"

#include <cstring>

namespace backhaul {

std::size_t i420Size(int width, int height) {
  if (width <= 0 || height <= 0) return 0;
  const std::size_t w = static_cast<std::size_t>(width), h = static_cast<std::size_t>(height);
  return w * h + 2 * ((w + 1) / 2) * ((h + 1) / 2);
}

bool nv12ToI420(const std::uint8_t* y, int yStride, const std::uint8_t* uv, int uvStride, int width, int height,
                std::uint8_t* out) {
  if (y == nullptr || uv == nullptr || out == nullptr || width <= 0 || height <= 0 || width > 16384 || height > 16384) {
    return false;
  }
  const std::size_t w = static_cast<std::size_t>(width), h = static_cast<std::size_t>(height);
  const std::size_t cw = (w + 1) / 2, ch = (h + 1) / 2;
  if (yStride < width || static_cast<std::size_t>(uvStride) < cw * 2) return false;

  for (std::size_t row = 0; row < h; ++row) {
    std::memcpy(out + row * w, y + row * static_cast<std::size_t>(yStride), w);
  }
  std::uint8_t* u = out + w * h;
  std::uint8_t* v = u + cw * ch;
  for (std::size_t row = 0; row < ch; ++row) {
    const std::uint8_t* pairs = uv + row * static_cast<std::size_t>(uvStride);
    for (std::size_t i = 0; i < cw; ++i) {
      u[row * cw + i] = pairs[i * 2];
      v[row * cw + i] = pairs[i * 2 + 1];
    }
  }
  return true;
}

FramePacer::FramePacer(double fps) : intervalUs_(fps > 0.0 ? static_cast<std::int64_t>(1000000.0 / fps) : 0) {}

bool FramePacer::accept(std::int64_t timeUs) {
  if (intervalUs_ <= 0) return true;
  // A little early still counts: capture times jitter around the exact rhythm.
  const std::int64_t slack = intervalUs_ / 8;
  if (!started_ || timeUs - nextUs_ > 4 * intervalUs_) {  // first picture, or back after a gap
    started_ = true;
    nextUs_ = timeUs + intervalUs_;
    return true;
  }
  if (timeUs + slack < nextUs_) return false;
  nextUs_ += intervalUs_;
  return true;
}

}  // namespace backhaul
