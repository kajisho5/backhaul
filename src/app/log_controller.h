// The show log: writes down what happens (see core/show_log.h) by watching the other
// parts, and saves it as a CSV file when the operator asks. Nothing is written anywhere
// on its own, and nothing is kept once the program is closed.
#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <functional>

#include "core/show_log.h"

class AppState;
class AudioController;
class MeetingController;

class LogController : public QObject {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY changed)
  Q_PROPERTY(bool canSave READ canSave CONSTANT)  // false where the system's file windows are not available
  Q_PROPERTY(QString message READ message NOTIFY changed)  // what the last save did
  Q_PROPERTY(bool failed READ failed NOTIFY changed)

 public:
  using Clock = std::function<qint64()>;  // milliseconds since 1970
  // `audio` may be null (a test without sound).
  LogController(AppState& app, MeetingController& meeting, AudioController* audio, Clock clock = {}, QObject* parent = nullptr);

  int count() const { return static_cast<int>(log_.size()); }
  bool canSave() const;
  QString message() const { return message_; }
  bool failed() const { return failed_; }

  void note(backhaul::LogKind kind, const QString& who = {});
  // The file as it is saved: UTF-8 with the mark spreadsheets want, times in this PC's zone.
  QByteArray csv() const;
  // Asks for a file name with the system's window, then saves.
  Q_INVOKABLE void save();
  bool saveTo(const QString& path);
  const backhaul::ShowLog& log() const { return log_; }

 signals:
  void changed();

 private:
  void lookAtMeeting();
  void lookAtPeople();
  void lookAtAudio();
  qint64 nowMs() const;

  AppState& app_;
  MeetingController& meeting_;
  AudioController* audio_;
  Clock clock_;
  backhaul::ShowLog log_;
  bool wasIn_ = false, listed_ = false, afterDrop_ = false;
  bool audioLost_ = false, onSpare_ = false;
  QHash<quint32, QString> people_;  // who was in the meeting at the last look (not this PC itself)
  QString message_;
  bool failed_ = false;
};
