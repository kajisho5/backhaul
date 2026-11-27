// Pictures from a capture device arrive as NV12 (a brightness plane followed by one
// plane of interleaved colour pairs). The meeting wants I420 (three separate planes).
#pragma once

#include <cstddef>
#include <cstdint>

namespace backhaul {

// Bytes of an I420 picture of this size (chroma planes are half size, rounded up).
std::size_t i420Size(int width, int height);

// `y` has `height` rows of `yStride` bytes, `uv` has (height + 1) / 2 rows of `uvStride`
// bytes; strides may be larger than the picture. `out` receives the three planes one
// after another without padding and must hold i420Size(width, height) bytes.
// Returns false and writes nothing for sizes or strides that make no sense.
bool nv12ToI420(const std::uint8_t* y, int yStride, const std::uint8_t* uv, int uvStride, int width, int height,
                std::uint8_t* out);

// Lets through at most `fps` pictures per second from a faster source (a 59.94 Hz
// capture into a 30-per-second send). Times are in microseconds; when they jump back
// (a device that was restarted) the rhythm simply starts again.
class FramePacer {
 public:
  explicit FramePacer(double fps);
  bool accept(std::int64_t timeUs);

 private:
  std::int64_t intervalUs_;
  std::int64_t nextUs_ = 0;
  std::int64_t lastUs_ = 0;
  bool started_ = false;
};

}  // namespace backhaul
