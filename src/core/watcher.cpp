#include "core/watcher.h"

#include "core/recovery.h"

namespace backhaul {

WatchEnd afterProgramEnded(const WatchHooks& hooks) {
  if (!hooks.crashed()) return WatchEnd::CleanExit;
  hooks.clearMark();
  const std::int64_t now = hooks.now();
  if (!mayRestart(hooks.earlier(), now)) return WatchEnd::GaveUp;
  // Without the note the limit could not be kept: better not to start than to start for ever.
  if (!hooks.note(now)) return WatchEnd::GaveUp;
  return hooks.startAgain() ? WatchEnd::StartedAgain : WatchEnd::StartFailed;
}

}  // namespace backhaul
