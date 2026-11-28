#include "watchdog.h"

#ifdef _WIN32
#include <windows.h>

#include <shellapi.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/watcher.h"

namespace watchdog {
namespace {

std::wstring wide(const std::string& utf8) {
  if (utf8.empty()) return {};
  const int need = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
  if (need <= 1) return {};
  std::wstring out(static_cast<std::size_t>(need), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, out.data(), need);
  out.resize(static_cast<std::size_t>(need) - 1);
  return out;
}

std::wstring ownPath() {
  std::wstring path(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  if (length == 0 || length >= path.size()) return {};
  path.resize(length);
  return path;
}

// Starts this program again with the given arguments, on its own (no console, not a child
// that ends with us). The handles are closed at once: nobody waits for it here.
bool launch(const std::wstring& arguments) {
  const std::wstring exe = ownPath();
  if (exe.empty()) return false;
  std::wstring command = L"\"" + exe + L"\" " + arguments;
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION started{};
  if (CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, nullptr,
                     nullptr, &startup, &started) == FALSE) {
    return false;
  }
  CloseHandle(started.hThread);
  CloseHandle(started.hProcess);
  return true;
}

std::wstring eventName(DWORD pid) { return L"Local\\backhaul-watch-" + std::to_wstring(pid); }

std::filesystem::path markOf(const std::filesystem::path& folder) { return folder / L"running"; }
std::filesystem::path listOf(const std::filesystem::path& folder) { return folder / L"restarts.txt"; }

std::int64_t nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// Set by begin(), used by end().
std::filesystem::path g_folder;
HANDLE g_goodbye = nullptr;

int watch(DWORD pid, const std::filesystem::path& folder) {
  // Opened first: if the program is already gone, there is nothing to watch.
  const HANDLE program = OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (program == nullptr) return 0;
  const HANDLE goodbye = CreateEventW(nullptr, TRUE, FALSE, eventName(pid).c_str());
  HANDLE both[2] = {program, goodbye};
  const DWORD woke = WaitForMultipleObjects(goodbye != nullptr ? 2 : 1, both, FALSE, INFINITE);
  CloseHandle(program);
  if (goodbye != nullptr) CloseHandle(goodbye);
  if (woke != WAIT_OBJECT_0) return 0;  // the program said goodbye (or the wait failed): nothing to do

  backhaul::WatchHooks hooks;
  hooks.crashed = [&folder] {
    std::error_code ignored;
    return std::filesystem::exists(markOf(folder), ignored);
  };
  hooks.clearMark = [&folder] {
    std::error_code ignored;
    std::filesystem::remove(markOf(folder), ignored);
  };
  hooks.now = [] { return nowMs(); };
  hooks.earlier = [&folder] {
    std::vector<std::int64_t> times;
    std::ifstream in(listOf(folder));
    std::int64_t at = 0;
    while (in >> at) times.push_back(at);
    return times;
  };
  hooks.note = [&folder](std::int64_t at) {
    // Only the last few matter; the file is written anew so that it never grows.
    std::vector<std::int64_t> times;
    {
      std::ifstream in(listOf(folder));
      std::int64_t earlier = 0;
      while (in >> earlier) times.push_back(earlier);
    }
    times.push_back(at);
    if (times.size() > 8) times.erase(times.begin(), times.end() - 8);
    std::ofstream out(listOf(folder), std::ios::trunc);
    for (const std::int64_t one : times) out << one << '\n';
    out.flush();
    return out.good();
  };
  hooks.startAgain = [] { return launch(L"--resume"); };
  return backhaul::afterProgramEnded(hooks) == backhaul::WatchEnd::StartFailed ? 1 : 0;
}

}  // namespace

// The arguments are read from Windows directly: what main() gets is in the system's old
// character set, which cannot hold every folder name.
bool ranAsWatcher(int, char*[], int* exitCode) {
  int count = 0;
  LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
  if (arguments == nullptr) return false;
  const bool watcher = count == 4 && std::wstring(arguments[1]) == L"--watch";
  int code = 2;
  if (watcher) {
    wchar_t* rest = nullptr;
    const unsigned long pid = std::wcstoul(arguments[2], &rest, 10);
    const std::wstring folder = arguments[3];
    if (rest != nullptr && *rest == L'\0' && pid != 0 && !folder.empty()) code = watch(static_cast<DWORD>(pid), folder);
  }
  LocalFree(arguments);
  if (watcher && exitCode != nullptr) *exitCode = code;
  return watcher;
}

bool begin(const std::string& folder) {
  const std::wstring where = wide(folder);
  if (where.empty()) return false;
  g_folder = where;
  std::error_code ignored;
  std::filesystem::create_directories(g_folder, ignored);
  const DWORD pid = GetCurrentProcessId();
  // Made here as well, so that end() can signal even if the watcher has not got that far yet.
  g_goodbye = CreateEventW(nullptr, TRUE, FALSE, eventName(pid).c_str());
  std::wstring quoted = where;
  if (!quoted.empty() && quoted.back() == L'\\') quoted.push_back(L'\\');  // a backslash before the closing quote would swallow it
  if (!launch(L"--watch " + std::to_wstring(pid) + L" \"" + quoted + L"\"")) return false;
  std::ofstream mark(markOf(g_folder), std::ios::trunc);
  mark << pid << '\n';
  return mark.good();
}

void end() {
  if (!g_folder.empty()) {
    std::error_code ignored;
    std::filesystem::remove(markOf(g_folder), ignored);
  }
  if (g_goodbye != nullptr) {
    SetEvent(g_goodbye);
    CloseHandle(g_goodbye);
    g_goodbye = nullptr;
  }
}

}  // namespace watchdog

#else

namespace watchdog {

bool ranAsWatcher(int, char*[], int*) { return false; }
bool begin(const std::string&) { return false; }
void end() {}

}  // namespace watchdog

#endif
