// Replacing this program by a newer version. Everything that touches the update library
// (WinSparkle) is in this folder, so that a change of that library is felt only here.
// The library downloads the installer, checks its signature against the key built into
// this program, and runs it. A build without the library has no updater.
#pragma once

#include <functional>
#include <memory>
#include <string>

namespace backhaul {

class Updater {
 public:
  /// Both are called from a thread of the update library, not from the caller's.
  struct Hooks {
    std::function<bool()> mayClose;  // false: the program must stay open now (it is in a meeting)
    std::function<void()> close;     // the installer is about to run: close the program
  };

  virtual ~Updater() = default;
  /// Once, before anything else. `version` is this program's own, like "0.1.0".
  virtual void start(const std::string& version, Hooks hooks) = 0;
  /// Downloads the newest version, checks it and installs it. Shows its own small window
  /// with the progress; the program is closed through `Hooks::close` when it is time.
  virtual void install() = 0;
};

/// Null when this build cannot update itself.
std::unique_ptr<Updater> makeUpdater();

}  // namespace backhaul
