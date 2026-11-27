// Tiny test helper: no third-party test framework needed.
#pragma once

#include <cmath>
#include <cstdio>

namespace testing {
inline int& failures() {
  static int n = 0;
  return n;
}
}  // namespace testing

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
      ++testing::failures();                                               \
    }                                                                      \
  } while (0)

#define CHECK_NEAR(a, b, tol)                                              \
  do {                                                                     \
    const double va_ = static_cast<double>(a);                             \
    const double vb_ = static_cast<double>(b);                             \
    if (!(std::fabs(va_ - vb_) <= (tol))) {                                \
      std::printf("FAIL %s:%d  %s = %g, expected %g\n", __FILE__, __LINE__, #a, va_, vb_); \
      ++testing::failures();                                               \
    }                                                                      \
  } while (0)
