#include "check.h"
#include "core/recovery.h"
#include "core/watcher.h"

using namespace backhaul;

void testWatcher() {
  bool crashed = false, canNote = true, canStart = true, markThere = true;
  std::vector<std::int64_t> noted;
  std::int64_t now = 1'000'000;
  int starts = 0;
  WatchHooks hooks;
  hooks.crashed = [&] { return crashed; };
  hooks.earlier = [&] { return noted; };
  hooks.now = [&] { return now; };
  hooks.note = [&](std::int64_t at) {
    if (canNote) noted.push_back(at);
    return canNote;
  };
  hooks.clearMark = [&] { markThere = false; };
  hooks.startAgain = [&] {
    if (canStart) ++starts;
    return canStart;
  };

  // A normal exit (also for an update): nothing happens, and the mark is not touched.
  CHECK(afterProgramEnded(hooks) == WatchEnd::CleanExit && starts == 0 && noted.empty() && markThere);

  // A crash: started again, and written down.
  crashed = true;
  CHECK(afterProgramEnded(hooks) == WatchEnd::StartedAgain && starts == 1 && noted.size() == 1 && !markThere);
  // The third crash within five minutes is left alone...
  now += 60'000;
  CHECK(afterProgramEnded(hooks) == WatchEnd::StartedAgain && starts == 2);
  now += 60'000;
  CHECK(afterProgramEnded(hooks) == WatchEnd::GaveUp && starts == 2 && noted.size() == 2);
  // ...and later it is allowed again.
  now += kRestartWindowMs;
  CHECK(afterProgramEnded(hooks) == WatchEnd::StartedAgain && starts == 3);

  // The note cannot be written: no start, or the limit could never be kept.
  noted.clear();
  canNote = false;
  CHECK(afterProgramEnded(hooks) == WatchEnd::GaveUp && starts == 3);
  // The start itself fails: said as such; the mark is gone, so this is not taken for a crash later.
  canNote = true;
  canStart = false;
  markThere = true;
  CHECK(afterProgramEnded(hooks) == WatchEnd::StartFailed && starts == 3 && !markThere);
}
