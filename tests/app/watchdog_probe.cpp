// A stand-in for the program, for the watcher test on Windows (see watchdog_test.py):
// it starts the real watcher from src/app/watchdog.cpp and then ends the way the test
// asks for. What it is to do comes in through the environment, which the watcher and the
// run it starts both inherit.
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "app/watchdog.h"

namespace {

std::wstring fromEnvironment(const wchar_t* name) {
  std::wstring value(32768, L'\0');
  const DWORD length = GetEnvironmentVariableW(name, value.data(), static_cast<DWORD>(value.size()));
  if (length == 0 || length >= value.size()) return {};
  value.resize(length);
  return value;
}

std::string utf8(const std::wstring& wide) {
  if (wide.empty()) return {};
  const int need = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (need <= 1) return {};
  std::string out(static_cast<std::size_t>(need), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, out.data(), need, nullptr, nullptr);
  out.resize(static_cast<std::size_t>(need) - 1);
  return out;
}

}  // namespace

int main(int argc, char* argv[]) {
  int watcherCode = 0;
  if (watchdog::ranAsWatcher(argc, argv, &watcherCode)) return watcherCode;

  const std::wstring folder = fromEnvironment(L"BACKHAUL_PROBE_FOLDER");
  const std::wstring plan = fromEnvironment(L"BACKHAUL_PROBE_PLAN");
  if (folder.empty() || plan.empty()) return 2;

  const std::wstring commandLine = GetCommandLineW();
  const bool resumed = commandLine.find(L"--resume") != std::wstring::npos;
  if (resumed) {
    std::ofstream list(std::filesystem::path(folder) / L"resumed.txt", std::ios::app);
    list << "resumed\n";
    list.flush();
    if (!list.good()) return 3;
    // Started again once is all the plain "crash" plan wants to see.
    if (plan != L"crash-always") return 0;
  }

  if (!watchdog::begin(utf8(folder))) return 4;
  Sleep(1500);  // the watcher needs a moment to find this run
  if (plan == L"clean") {
    watchdog::end();
    return 0;
  }
  // Ends like a crash does for the watcher: at once, without saying goodbye. (A real
  // crash would bring up Windows' own error reporting, which a test must not wait for.)
  TerminateProcess(GetCurrentProcess(), 3);
  return 5;
}
