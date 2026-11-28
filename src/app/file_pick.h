// The system's own "save as" and "open" windows. Only on Windows; elsewhere
// canPickFiles() is false and both functions give an empty name.
#pragma once

#include <QString>

// What kind of file the window is for: it shows files of that kind and adds the ending
// when the operator types a name without one.
enum class FileKind { Project, Csv };

bool canPickFiles();
// Empty when the operator closed the window without choosing.
QString pickSaveFile(const QString& title, const QString& suggestedName, FileKind kind = FileKind::Project);
QString pickOpenFile(const QString& title);
