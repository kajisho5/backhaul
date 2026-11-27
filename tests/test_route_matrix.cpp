#include <vector>

#include "check.h"
#include "core/route_matrix.h"

using namespace backhaul;

void testRouteMatrix() {
  const std::size_t frames = 4;
  std::vector<float> a{1, 1, 1, 1}, b{0.5f, 0.5f, 0.5f, 0.5f};
  std::vector<float> o0(frames, 9), o1(frames, 9), o2(frames, 9);
  float* out[] = {o0.data(), o1.data(), o2.data()};

  {  // one slot per channel; unrouted channels are silent even if they held old data
    RouteMatrix m(2, 3);
    CHECK(m.setRoute(0, 0, true));
    CHECK(m.setRoute(1, 1, true));
    const float* in[] = {a.data(), b.data()};
    m.mix(in, out, frames);
    CHECK_NEAR(o0[0], 1.0, 1e-6); CHECK_NEAR(o1[3], 0.5, 1e-6); CHECK_NEAR(o2[2], 0.0, 1e-6);
    CHECK(m.isOn(0, 0) && !m.isOn(0, 1));
  }
  {  // two slots sharing one channel are summed with their own levels
    RouteMatrix m(2, 3);
    m.setRoute(0, 2, true, -6.0f);
    m.setRoute(1, 2, true, 6.0f);
    const float* in[] = {a.data(), b.data()};
    m.mix(in, out, frames);
    CHECK_NEAR(o2[0], 0.501187 + 0.5 * 1.995262, 1e-4);
    CHECK_NEAR(o0[0], 0.0, 1e-6);
  }
  {  // switching a route off again
    RouteMatrix m(1, 1);
    m.setRoute(0, 0, true);
    m.setRoute(0, 0, false);
    const float* in[] = {a.data()};
    float* one[] = {o0.data()};
    m.mix(in, one, frames);
    CHECK_NEAR(o0[1], 0.0, 1e-6);
    CHECK(!m.isOn(0, 0));
  }
  {  // a slot with no audio right now (null) is silence, not a crash
    RouteMatrix m(2, 1);
    m.setRoute(0, 0, true);
    m.setRoute(1, 0, true);
    const float* in[] = {nullptr, b.data()};
    float* one[] = {o0.data()};
    m.mix(in, one, frames);
    CHECK_NEAR(o0[0], 0.5, 1e-6);
  }
  {  // out-of-range indexes are refused
    RouteMatrix m(2, 2);
    CHECK(!m.setRoute(2, 0, true));
    CHECK(!m.setRoute(0, 2, true));
    CHECK(!m.isOn(5, 5));
  }
  CHECK_NEAR(RouteMatrix::dbToLinear(0.0f), 1.0, 1e-6);
  CHECK_NEAR(RouteMatrix::dbToLinear(-20.0f), 0.1, 1e-6);
}
