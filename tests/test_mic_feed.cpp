#include <cstdint>
#include <limits>
#include <vector>

#include "check.h"
#include "core/mic_feed.h"

using backhaul::MicFeed;

void testMicFeed() {
  {  // what goes in comes out in order; stereo keeps the sides apart, mono averages them
    MicFeed feed(256);
    const std::vector<float> in{0.5f, -0.5f, 0.25f, 0.75f, -1.0f, 1.0f};  // 3 frames, 2 channels
    feed.write(in.data(), 3, 2);
    CHECK(feed.available() == 3);
    std::int16_t out[8] = {};
    CHECK(feed.read(out, 2, true) == 2);
    CHECK(out[0] == 16384 && out[1] == -16384);  // 0.5 * 32767 rounded
    CHECK(out[2] == 8192 && out[3] == 24575);
    CHECK(feed.read(out, 4, false) == 1);
    CHECK(out[0] == 0);  // (-1 + 1) / 2
    CHECK(feed.read(out, 4, true) == 0);
  }
  {  // a device with many channels: the chosen pair is taken; a single channel fills both sides
    MicFeed feed(256);
    const std::vector<float> in{0.1f, 0.2f, 0.3f, 0.4f};  // 1 frame, 4 channels
    feed.write(in.data(), 1, 4, 2);
    std::int16_t out[2] = {};
    CHECK(feed.read(out, 1, true) == 1);
    CHECK(out[0] == 9830 && out[1] == 13107);
    feed.write(in.data(), 1, 4, 3);  // the last channel has no partner
    CHECK(feed.read(out, 1, true) == 1);
    CHECK(out[0] == 13107 && out[1] == 13107);
    const float mono[2] = {0.5f, -0.25f};
    feed.write(mono, 2, 1);
    CHECK(feed.read(out, 1, true) == 1);
    CHECK(out[0] == 16384 && out[1] == 16384);
    feed.clear();
    // Requests that make no sense change nothing.
    feed.write(in.data(), 1, 4, 4);
    feed.write(in.data(), 1, 4, -1);
    feed.write(nullptr, 1, 2);
    feed.write(in.data(), 1, 0);
    CHECK(feed.available() == 0);
    CHECK(feed.read(nullptr, 4, true) == 0);
  }
  {  // too-loud samples are held to full scale, broken ones are silence; meters show the loudest since last asked
    MicFeed feed(256);
    const float in[6] = {2.0f, -3.0f, std::numeric_limits<float>::quiet_NaN(), 0.5f, 0.1f, -0.2f};
    feed.write(in, 3, 2);
    std::int16_t out[6] = {};
    CHECK(feed.read(out, 3, true) == 3);
    CHECK(out[0] == 32767 && out[1] == -32767);
    CHECK(out[2] == 0 && out[3] == 16384);  // a broken sample is silence, never a click at full level
    CHECK_NEAR(feed.takePeak(0), 1.0, 1e-6);
    CHECK_NEAR(feed.takePeak(1), 1.0, 1e-6);
    CHECK(feed.takePeak(0) == 0.0f);
    const float quiet[2] = {0.25f, -0.5f};
    feed.write(quiet, 1, 2);
    CHECK_NEAR(feed.takePeak(0), 0.25, 1e-6);
    CHECK_NEAR(feed.takePeak(1), 0.5, 1e-6);
    CHECK(feed.takePeak(2) == 0.0f && feed.takePeak(-1) == 0.0f);
  }
  {  // when nobody takes the sound out, the newest is dropped and nothing is overwritten
    MicFeed feed(64);
    std::vector<float> in(100 * 2);
    for (std::size_t i = 0; i < 100; ++i) in[i * 2] = in[i * 2 + 1] = static_cast<float>(i) / 1000.0f;
    feed.write(in.data(), 100, 2);
    CHECK(feed.available() == 64);
    std::vector<std::int16_t> out(64);
    CHECK(feed.read(out.data(), 64, false) == 64);
    CHECK(out[0] == 0 && out[63] == 2064);  // 0.063 * 32767
    // The ring keeps working across its end.
    feed.write(in.data(), 40, 2);
    CHECK(feed.read(out.data(), 64, false) == 40);
    CHECK(out[39] == 1278);
    feed.write(in.data(), 10, 2);
    feed.clear();
    CHECK(feed.available() == 0);
  }
}
