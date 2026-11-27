#include "core/yuv.h"

namespace backhaul {
namespace {

std::uint8_t clamp8(int value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return static_cast<std::uint8_t>(value);
}

// BT.709 coefficients in 16.16 fixed point.
struct Matrix {
  int yScale, yOffset, rv, gu, gv, bu;
};
constexpr Matrix kLimited{76309, 16, 117489, 13975, 34925, 138438};  // 1.1644, 1.7927, 0.2132, 0.5329, 2.1124
constexpr Matrix kFull{65536, 0, 103206, 12276, 30679, 121609};      // 1, 1.5748, 0.1873, 0.4681, 1.8556

}  // namespace

void i420ToBgra(const std::uint8_t* y, const std::uint8_t* u, const std::uint8_t* v, int width, int height,
                bool limitedRange, std::uint8_t* out) {
  if (width <= 0 || height <= 0) return;
  const Matrix& m = limitedRange ? kLimited : kFull;
  const int chromaWidth = (width + 1) / 2;
  for (int row = 0; row < height; ++row) {
    const std::uint8_t* yRow = y + static_cast<std::ptrdiff_t>(row) * width;
    const std::uint8_t* uRow = u + static_cast<std::ptrdiff_t>(row / 2) * chromaWidth;
    const std::uint8_t* vRow = v + static_cast<std::ptrdiff_t>(row / 2) * chromaWidth;
    std::uint8_t* dst = out + static_cast<std::ptrdiff_t>(row) * width * 4;
    for (int col = 0; col < width; ++col) {
      const int luma = (static_cast<int>(yRow[col]) - m.yOffset) * m.yScale;
      const int cb = static_cast<int>(uRow[col / 2]) - 128;
      const int cr = static_cast<int>(vRow[col / 2]) - 128;
      dst[0] = clamp8((luma + m.bu * cb + 32768) >> 16);
      dst[1] = clamp8((luma - m.gu * cb - m.gv * cr + 32768) >> 16);
      dst[2] = clamp8((luma + m.rv * cr + 32768) >> 16);
      dst[3] = 255;
      dst += 4;
    }
  }
}

}  // namespace backhaul
