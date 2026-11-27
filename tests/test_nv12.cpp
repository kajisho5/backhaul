#include <cstdint>
#include <vector>

#include "check.h"
#include "core/nv12.h"

using namespace backhaul;

void testNv12() {
  CHECK(i420Size(4, 2) == 8 + 2 * 2);
  CHECK(i420Size(5, 3) == 15 + 2 * 3 * 2);  // odd sizes round the colour planes up
  CHECK(i420Size(0, 2) == 0 && i420Size(4, -1) == 0);

  {  // planes are taken apart, and padding at the end of rows is skipped
    const int w = 4, h = 2, stride = 6;
    const std::vector<std::uint8_t> y{1, 2, 3, 4, 99, 99, 5, 6, 7, 8, 99, 99};
    const std::vector<std::uint8_t> uv{10, 20, 11, 21, 99, 99};
    std::vector<std::uint8_t> out(i420Size(w, h), 0);
    CHECK(nv12ToI420(y.data(), stride, uv.data(), stride, w, h, out.data()));
    const std::vector<std::uint8_t> want{1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 20, 21};
    CHECK(out == want);
  }
  {  // odd width and height
    const std::vector<std::uint8_t> y{1, 2, 3, 4, 5, 6, 7, 8, 9};
    const std::vector<std::uint8_t> uv{10, 20, 11, 21, 12, 22, 13, 23};
    std::vector<std::uint8_t> out(i420Size(3, 3), 0);
    CHECK(nv12ToI420(y.data(), 3, uv.data(), 4, 3, 3, out.data()));
    const std::vector<std::uint8_t> want{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 20, 21, 22, 23};
    CHECK(out == want);
  }
  {  // nonsense is refused and nothing is written
    std::vector<std::uint8_t> in(64, 1), out(64, 7);
    CHECK(!nv12ToI420(nullptr, 4, in.data(), 4, 4, 2, out.data()));
    CHECK(!nv12ToI420(in.data(), 4, nullptr, 4, 4, 2, out.data()));
    CHECK(!nv12ToI420(in.data(), 4, in.data(), 4, 4, 2, nullptr));
    CHECK(!nv12ToI420(in.data(), 3, in.data(), 4, 4, 2, out.data()));   // rows shorter than the picture
    CHECK(!nv12ToI420(in.data(), 4, in.data(), 3, 4, 2, out.data()));
    CHECK(!nv12ToI420(in.data(), 4, in.data(), 4, 0, 2, out.data()));
    CHECK(!nv12ToI420(in.data(), 4, in.data(), 4, 4, 20000, out.data()));
    CHECK(out == std::vector<std::uint8_t>(64, 7));
  }

  {  // 59.94 pictures per second in, 30 out: every second one
    FramePacer pacer(30.0);
    int passed = 0;
    for (int i = 0; i < 600; ++i) passed += pacer.accept(static_cast<std::int64_t>(i) * 16683) ? 1 : 0;
    CHECK(passed >= 298 && passed <= 302);
  }
  {  // a 30-per-second source passes whole, even with jittery times; a slower one too
    FramePacer pacer(30.0);
    int passed = 0;
    for (int i = 0; i < 300; ++i) passed += pacer.accept(static_cast<std::int64_t>(i) * 33333 + (i % 2 == 0 ? 900 : -900)) ? 1 : 0;
    CHECK(passed >= 298);
    FramePacer slow(30.0);
    passed = 0;
    for (int i = 0; i < 100; ++i) passed += slow.accept(static_cast<std::int64_t>(i) * 66667) ? 1 : 0;
    CHECK(passed == 100);
  }
  {  // after a long gap the rhythm starts again instead of letting a burst through
    FramePacer pacer(30.0);
    CHECK(pacer.accept(0));
    CHECK(!pacer.accept(10000));
    CHECK(pacer.accept(5000000));
    CHECK(!pacer.accept(5010000));
    CHECK(pacer.accept(5033400));
    CHECK(pacer.accept(1000));      // the device's clock started over: carry on from there
    CHECK(!pacer.accept(11000));
    CHECK(pacer.accept(34400));
    FramePacer all(0.0);  // no limit
    CHECK(all.accept(0) && all.accept(1) && all.accept(2));
  }
}
