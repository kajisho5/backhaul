#include "file_pick.h"

#ifdef _WIN32
#include <windows.h>

#include <commdlg.h>

namespace {

const wchar_t kFilter[] = L"backhaul project (*.backhaul)\0*.backhaul\0All files (*.*)\0*.*\0";

QString pick(const QString& title, const QString& suggestedName, bool save) {
  wchar_t path[4096] = {};
  const std::wstring name = suggestedName.toStdWString();
  if (name.size() < 4000) name.copy(path, name.size());
  const std::wstring caption = title.toStdWString();
  OPENFILENAMEW ask{};
  ask.lStructSize = sizeof(ask);
  ask.hwndOwner = GetActiveWindow();
  ask.lpstrFilter = kFilter;
  ask.lpstrFile = path;
  ask.nMaxFile = static_cast<DWORD>(sizeof(path) / sizeof(path[0]));
  ask.lpstrTitle = caption.c_str();
  ask.lpstrDefExt = L"backhaul";
  ask.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | (save ? OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST : OFN_FILEMUSTEXIST);
  const BOOL chosen = save ? GetSaveFileNameW(&ask) : GetOpenFileNameW(&ask);
  return chosen ? QString::fromWCharArray(path) : QString();
}

}  // namespace

bool canPickFiles() { return true; }
QString pickSaveFile(const QString& title, const QString& suggestedName) { return pick(title, suggestedName, true); }
QString pickOpenFile(const QString& title) { return pick(title, QString(), false); }

#else

bool canPickFiles() { return false; }
QString pickSaveFile(const QString&, const QString&) { return QString(); }
QString pickOpenFile(const QString&) { return QString(); }

#endif
