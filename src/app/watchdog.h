// Starting again after a crash. While the program runs, a second, tiny run of the same
// program (the watcher) waits for it to end; if it ended without saying goodbye, the
// watcher starts it again with "--resume" (see core/watcher.h for when it does not).
// Everything that touches Windows for this is in watchdog.cpp; elsewhere nothing happens.
#pragma once

#include <string>

namespace watchdog {

// For main(): when this run was started as the watcher ("--watch <pid> <folder>"), does
// the watching and returns true with the exit code in `exitCode`. Call before anything
// else: the watcher needs neither a window nor the settings.
bool ranAsWatcher(int argc, char* argv[], int* exitCode);

// In the program. `folder` is where the "running" mark and the list of restarts are kept
// (UTF-8). Starts the watcher and sets the mark; false when this system has no watcher.
bool begin(const std::string& folder);
// A normal exit: takes the mark away and lets the watcher go at once (an installer must
// not find any run of the program left).
void end();

}  // namespace watchdog
