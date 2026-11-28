// What the watcher does once the program it watches has ended: nothing after a normal
// exit; after a crash it starts the program again, unless that has happened too often.
// The watcher itself (a second, tiny run of the program) lives in src/app/watchdog.cpp;
// this is its decision, kept apart so that it can be tested.
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace backhaul {

struct WatchHooks {
  // True when the program ended without saying goodbye (its "running" mark is still there).
  std::function<bool()> crashed;
  // The times of earlier restarts, and the current time, in milliseconds.
  std::function<std::vector<std::int64_t>()> earlier;
  std::function<std::int64_t()> now;
  // Writes the time of this restart down. False when that did not work.
  std::function<bool(std::int64_t)> note;
  // Takes the "running" mark away, so that a failed start is not taken for another crash.
  std::function<void()> clearMark;
  // Starts the program again. False when that did not work.
  std::function<bool()> startAgain;
};

enum class WatchEnd { CleanExit, StartedAgain, GaveUp, StartFailed };

WatchEnd afterProgramEnded(const WatchHooks& hooks);

}  // namespace backhaul
