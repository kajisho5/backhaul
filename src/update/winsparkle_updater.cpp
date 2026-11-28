// The updater on Windows, with WinSparkle (https://winsparkle.org).
#include <winsparkle.h>

#include <mutex>
#include <string>

#include "update/updater.h"

namespace backhaul {
namespace {

// WinSparkle's callbacks are plain functions without a "user data" argument, so what
// they call is kept here. There is one updater per program.
std::mutex hooksMutex;
Updater::Hooks hooks;

int __cdecl mayClose() {
  std::function<bool()> ask;
  {
    const std::lock_guard<std::mutex> lock(hooksMutex);
    ask = hooks.mayClose;
  }
  return ask && ask() ? 1 : 0;  // nobody to ask: stay open
}

void __cdecl closeNow() {
  std::function<void()> close;
  {
    const std::lock_guard<std::mutex> lock(hooksMutex);
    close = hooks.close;
  }
  if (close) close();
}

std::wstring wide(const std::string& ascii) { return std::wstring(ascii.begin(), ascii.end()); }

class WinSparkleUpdater final : public Updater {
 public:
  ~WinSparkleUpdater() override {
    if (started_) win_sparkle_cleanup();
    const std::lock_guard<std::mutex> lock(hooksMutex);
    hooks = {};
  }

  void start(const std::string& version, Hooks given) override {
    if (started_) return;
    {
      const std::lock_guard<std::mutex> lock(hooksMutex);
      hooks = std::move(given);
    }
    win_sparkle_set_appcast_url(BACKHAUL_UPDATE_FEED_URL);
    // An installer whose signature does not match this key is not run.
    if (win_sparkle_set_eddsa_public_key(BACKHAUL_UPDATE_PUBLIC_KEY) != 1) return;
    win_sparkle_set_app_details(L"SEVENTHWELL", L"backhaul", wide(version).c_str());
    // Never on its own: this program decides when to look (not during a meeting).
    win_sparkle_set_automatic_check_for_updates(0);
    win_sparkle_set_can_shutdown_callback(mayClose);
    win_sparkle_set_shutdown_request_callback(closeNow);
    win_sparkle_init();
    started_ = true;
  }

  void install() override {
    if (started_) win_sparkle_check_update_with_ui_and_install();
  }

 private:
  bool started_ = false;
};

}  // namespace

std::unique_ptr<Updater> makeUpdater() { return std::make_unique<WinSparkleUpdater>(); }

}  // namespace backhaul
