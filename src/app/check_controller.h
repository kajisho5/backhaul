// The check before the show: gathers what is known about this PC right now, has
// core/preshow_check judge it, and puts the result into words for the settings page.
// Looking changes nothing.
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <functional>

#include "core/preshow_check.h"
#include "system_facts.h"

class AppState;
class AudioController;
class LicenceController;
class MeetingController;
class UpdateController;

class CheckController : public QObject {
  Q_OBJECT
  // One entry per point: { name, text, level } with level "ok" | "warn" | "bad".
  Q_PROPERTY(QVariantList items READ items NOTIFY changed)
  Q_PROPERTY(int okCount READ okCount NOTIFY changed)
  Q_PROPERTY(int warnCount READ warnCount NOTIFY changed)
  Q_PROPERTY(int badCount READ badCount NOTIFY changed)
  Q_PROPERTY(bool running READ running NOTIFY changed)      // the licence server is being asked
  Q_PROPERTY(QString checkedAt READ checkedAt NOTIFY changed)  // "14:02", empty before the first look

 public:
  using System = std::function<SystemFacts()>;
  using Clock = std::function<QString()>;
  // `licence`, `audio` and `updates` may be null (a part this build or this start does not have).
  CheckController(AppState& app, MeetingController& meeting, LicenceController* licence, AudioController* audio,
                  UpdateController* updates, System system = {}, Clock clock = {}, QObject* parent = nullptr);

  QVariantList items() const { return items_; }
  int okCount() const { return count(backhaul::CheckLevel::Ok); }
  int warnCount() const { return count(backhaul::CheckLevel::Warn); }
  int badCount() const { return count(backhaul::CheckLevel::Bad); }
  bool running() const { return running_; }
  QString checkedAt() const { return checkedAt_; }

  // Looks at everything again. The answer of the licence server arrives a moment later.
  Q_INVOKABLE void run();
  // For pictures: a result with every kind of answer in it.
  void showForPicture();

 signals:
  void changed();

 private:
  backhaul::ShowFacts gather() const;
  void show(const backhaul::ShowFacts& facts);
  QString nameOf(backhaul::CheckId id) const;
  QString textOf(const backhaul::CheckItem& item, const backhaul::ShowFacts& facts) const;
  int count(backhaul::CheckLevel level) const;

  AppState& app_;
  MeetingController& meeting_;
  LicenceController* licence_;
  AudioController* audio_;
  UpdateController* updates_;
  System system_;
  Clock clock_;
  std::vector<backhaul::CheckItem> judged_;
  QVariantList items_;
  QString checkedAt_;
  bool running_ = false;
  bool sample_ = false;  // the result on the screen is the one made for pictures
  int round_ = 0;
  bool netAsked_ = false, netReachable_ = false;
  int netMs_ = 0;
};
