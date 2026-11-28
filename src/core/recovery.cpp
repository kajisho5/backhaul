#include "core/recovery.h"

namespace backhaul {

bool shouldRejoin(EndCause cause) { return cause == EndCause::Connection; }

int rejoinWaitSeconds(int attempt) {
  if (attempt <= 1) return 5;
  if (attempt == 2) return 10;
  if (attempt == 3) return 20;
  return 30;
}

OutputPick pickOutput(bool mainThere, bool spareThere, std::int64_t mainGoneMs) {
  if (mainThere) return OutputPick::Main;
  if (spareThere && mainGoneMs >= kSpareAfterMs) return OutputPick::Spare;
  return OutputPick::None;
}

bool mayRestart(const std::vector<std::int64_t>& earlier, std::int64_t nowMs) {
  int recent = 0;
  for (const std::int64_t at : earlier) {
    if (at <= nowMs && nowMs - at < kRestartWindowMs) ++recent;
  }
  return recent + 1 < kCrashLimit;
}

}  // namespace backhaul
