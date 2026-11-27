#include <array>
#include <cstdint>
#include <vector>

#include "check.h"
#include "core/yuv.h"

namespace {

std::array<int, 3> one(int y, int u, int v, bool limited) {
  const std::uint8_t py = static_cast<std::uint8_t>(y), pu = static_cast<std::uint8_t>(u), pv = static_cast<std::uint8_t>(v);
  std::uint8_t out[4] = {};
  backhaul::i420ToBgra(&py, &pu, &pv, 1, 1, limited, out);
  CHECK(out[3] == 255);
  return {out[2], out[1], out[0]};  // R, G, B
}

bool near3(const std::array<int, 3>& got, int r, int g, int b) {
  auto close = [](int a, int e) { return a >= e - 1 && a <= e + 1; };
  return close(got[0], r) && close(got[1], g) && close(got[2], b);
}

}  // namespace

void testYuv() {
  // Video levels: 16 is black, 235 is white, greys stay grey.
  CHECK(near3(one(16, 128, 128, true), 0, 0, 0));
  CHECK(near3(one(235, 128, 128, true), 255, 255, 255));
  CHECK(near3(one(126, 128, 128, true), 128, 128, 128));
  // BT.709 primaries at video levels (values from the standard's matrix).
  CHECK(near3(one(63, 102, 240, true), 255, 0, 0));
  CHECK(near3(one(173, 42, 26, true), 0, 255, 0));
  CHECK(near3(one(32, 240, 118, true), 0, 0, 255));
  // Out-of-range input is clamped, not wrapped.
  CHECK(near3(one(255, 128, 128, true), 255, 255, 255));
  CHECK(near3(one(0, 128, 128, true), 0, 0, 0));
  // Full range.
  CHECK(near3(one(0, 128, 128, false), 0, 0, 0));
  CHECK(near3(one(255, 128, 128, false), 255, 255, 255));
  CHECK(near3(one(54, 99, 255, false), 255, 0, 0));

  // 3x3 picture: odd sizes use (n + 1) / 2 chroma samples; each chroma sample covers 2x2 pixels.
  const std::vector<std::uint8_t> y(9, 126);
  const std::uint8_t u[4] = {128, 128, 128, 240};
  const std::uint8_t v[4] = {128, 128, 128, 118};
  std::vector<std::uint8_t> out(9 * 4, 7);
  backhaul::i420ToBgra(y.data(), u, v, 3, 3, true, out.data());
  CHECK(out[0] == out[1] && out[1] == out[2]);                 // top-left: grey
  const std::uint8_t* last = &out[8 * 4];                      // bottom-right: uses the 4th chroma sample
  CHECK(last[0] > last[2] + 100);                              // strongly blue
  const std::uint8_t* mid = &out[4 * 4];                       // centre still uses the 1st sample
  CHECK(mid[0] == mid[2]);

  // Nothing is written for an empty picture.
  std::uint8_t guard[4] = {9, 9, 9, 9};
  backhaul::i420ToBgra(y.data(), u, v, 0, 3, true, guard);
  CHECK(guard[0] == 9);
}
