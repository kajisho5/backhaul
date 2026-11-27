// Reads the settings file at start-up and writes it again shortly after each change.
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class AppState;

class SettingsSaver : public QObject {
  Q_OBJECT
 public:
  enum class LoadResult {
    Loaded,       // the file was read
    NoFile,       // first run: defaults are in use
    Unreadable,   // the file exists but is damaged; it was set aside and defaults are in use
  };

  // `path` is the settings file. Where settings normally live: defaultPath().
  SettingsSaver(AppState& app, QString path, QObject* parent = nullptr);

  static QString defaultPath();

  // Fills `app` from the file, or with defaults. A damaged file is renamed (kept for
  // the user), never overwritten; its new name is in asidePath().
  LoadResult load();
  QString asidePath() const { return asidePath_; }

  // Writes now if a change is waiting. Returns false when writing failed.
  bool flush();

 private:
  bool write();

  AppState& app_;
  QString path_;
  QString asidePath_;
  QTimer timer_;
  bool pending_ = false;
};
