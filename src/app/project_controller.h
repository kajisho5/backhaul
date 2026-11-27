// A project is everything that is set up for one event, in one file the operator keeps:
// the role, the slots, pictures and sound, and the plug-ins with their settings.
// Zoom keys, passcodes and the like are never part of it (they are not in the settings either).
#pragma once

#include <QObject>
#include <QString>
#include <functional>

class AppState;
class PluginController;

class ProjectController : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool canPick READ canPick CONSTANT)  // false where the system's file windows are not available
  Q_PROPERTY(QString message READ message NOTIFY changed)  // what the last save or open did
  Q_PROPERTY(bool failed READ failed NOTIFY changed)
  Q_PROPERTY(QString fileName READ fileName NOTIFY changed)  // the project last saved or opened

 public:
  // `afterOpen` restarts whatever depends on the settings (sound and capture devices).
  ProjectController(AppState& app, PluginController& plugins, std::function<void()> afterOpen, QObject* parent = nullptr);

  bool canPick() const;
  QString message() const { return message_; }
  bool failed() const { return failed_; }
  QString fileName() const { return fileName_; }

  // Ask for a file name with the system's window, then save or open.
  Q_INVOKABLE void save();
  Q_INVOKABLE void open();

  bool saveTo(const QString& path);
  // On failure nothing is changed: the running set-up stays as it is.
  bool openFrom(const QString& path);

 signals:
  void changed();

 private:
  void say(const QString& message, bool failed);

  AppState& app_;
  PluginController& plugins_;
  std::function<void()> afterOpen_;
  QString message_, fileName_;
  bool failed_ = false;
};
