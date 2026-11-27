// The system's own "save as" and "open" windows. Only on Windows; elsewhere
// canPickFiles() is false and both functions give an empty name.
#pragma once

#include <QString>

bool canPickFiles();
// Empty when the operator closed the window without choosing.
QString pickSaveFile(const QString& title, const QString& suggestedName);
QString pickOpenFile(const QString& title);
