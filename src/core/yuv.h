// Planar YUV 4:2:0 (I420, BT.709) to 32-bit pixels in B, G, R, 255 byte order.
#pragma once

#include <cstdint>

namespace backhaul {

// `out` must hold width * height * 4 bytes. The chroma planes are (width + 1) / 2 wide
// and (height + 1) / 2 high, rows packed without padding (as the Zoom SDK delivers them).
// limitedRange: Y 16..235 and chroma 16..240 ("video levels"); otherwise 0..255.
void i420ToBgra(const std::uint8_t* y, const std::uint8_t* u, const std::uint8_t* v, int width, int height,
                bool limitedRange, std::uint8_t* out);

}  // namespace backhaul
