#include "core/line_quality.h"

#include <iterator>

namespace backhaul {

LineGrade gradeOfStep(int step) {
  if (step == 1 || step == 2) return LineGrade::Poor;
  if (step == 3) return LineGrade::Fair;
  if (step >= 4 && step <= 6) return LineGrade::Good;
  return LineGrade::Unknown;
}

LineGrade worse(LineGrade a, LineGrade b) {
  if (a == LineGrade::Unknown) return b;
  if (b == LineGrade::Unknown) return a;
  return static_cast<int>(a) > static_cast<int>(b) ? a : b;  // the grades are listed from good to poor
}

void UnstableWatch::report(std::uint32_t user, bool poor, std::int64_t nowMs) {
  if (!poor) {
    poorSince_.erase(user);
  } else {
    poorSince_.emplace(user, nowMs);  // keeps the first moment while it stays poor
  }
}

void UnstableWatch::keepOnly(const std::vector<std::uint32_t>& users) {
  for (auto it = poorSince_.begin(); it != poorSince_.end();) {
    bool here = false;
    for (const std::uint32_t user : users) here = here || user == it->first;
    it = here ? std::next(it) : poorSince_.erase(it);
  }
}

bool UnstableWatch::unstable(std::uint32_t user, std::int64_t nowMs) const {
  const auto found = poorSince_.find(user);
  return found != poorSince_.end() && nowMs - found->second >= kUnstableAfterMs;
}

}  // namespace backhaul
