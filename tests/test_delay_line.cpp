#include <vector>

#include "check.h"
#include "core/delay_line.h"

using namespace backhaul;

void testDelayLine() {
  {  // zero delay passes audio straight through
    DelayLine d(100);
    std::vector<float> in{1, 2, 3, 4}, out(4);
    d.process(in.data(), out.data(), 4);
    CHECK(out == in);
  }
  {  // delay of 3 samples, fed in uneven chunks
    DelayLine d(10);
    d.setDelay(3);
    std::vector<float> in{1, 2, 3, 4, 5, 6, 7}, out(7);
    d.process(in.data(), out.data(), 2);
    d.process(in.data() + 2, out.data() + 2, 5);
    const std::vector<float> want{0, 0, 0, 1, 2, 3, 4};
    CHECK(out == want);
  }
  {  // in-place processing
    DelayLine d(4);
    d.setDelay(1);
    std::vector<float> buf{5, 6, 7};
    d.process(buf.data(), buf.data(), 3);
    const std::vector<float> want{0, 5, 6};
    CHECK(buf == want);
  }
  {  // asking for more than the maximum is clamped, not out of bounds
    DelayLine d(2);
    d.setDelay(1000);
    CHECK(d.delay() == 2);
    std::vector<float> in{1, 2, 3, 4}, out(4);
    d.process(in.data(), out.data(), 4);
    const std::vector<float> want{0, 0, 1, 2};
    CHECK(out == want);
  }
  {  // maximum delay of zero still works
    DelayLine d(0);
    d.setDelay(5);
    std::vector<float> in{9, 8}, out(2);
    d.process(in.data(), out.data(), 2);
    CHECK(out == in);
  }
}
