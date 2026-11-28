// Updates as the screens see them: is there a newer version, may it be installed now.
// Looking is done here (a small file on the web says which version is the newest);
// installing is the updater's job (src/update), which checks the installer's signature.
// Nothing happens while this PC is in a meeting: no look, no notice, no install.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <atomic>
#include <functional>
#include <memory>

#include "update/updater.h"

class AppState;

class UpdateController : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
  Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
  Q_PROPERTY(QString notes READ notes NOTIFY changed)          // what is new, in the language shown
  Q_PROPERTY(QString status READ status NOTIFY changed)        // "idle" | "checking" | "current" | "available" | "failed"
  Q_PROPERTY(QString checkedAt READ checkedAt NOTIFY changed)  // "14:02", empty before the first look
  Q_PROPERTY(bool noticeVisible READ noticeVisible NOTIFY changed)
  Q_PROPERTY(bool canCheck READ canCheck NOTIFY changed)
  Q_PROPERTY(bool canInstall READ canInstall NOTIFY changed)
  Q_PROPERTY(bool selfUpdating READ selfUpdating CONSTANT)     // false: this build has no updater

 public:
  /// Fetches a small file; calls back once, with ok=false when it could not be had.
  using Fetch = std::function<void(const QUrl&, std::function<void(bool ok, const QByteArray& body)>)>;
  using Clock = std::function<QString()>;  // the time of day as shown, like "14:02"

  /// `updater` and `fetch` may be null: then nothing can be installed / looked up.
  UpdateController(AppState& app, QString currentVersion, QUrl latestUrl, std::unique_ptr<backhaul::Updater> updater,
                   Fetch fetch, Clock clock = {}, QObject* parent = nullptr);

  QString currentVersion() const { return currentVersion_; }
  QString latestVersion() const { return latestVersion_; }
  QString notes() const;
  QString status() const { return status_; }
  QString checkedAt() const { return checkedAt_; }
  bool noticeVisible() const;
  bool canCheck() const { return fetch_ != nullptr && !inMeeting_ && status_ != QStringLiteral("checking"); }
  bool canInstall() const { return updater_ != nullptr && !inMeeting_ && status_ == QStringLiteral("available"); }
  bool selfUpdating() const { return updater_ != nullptr; }

  /// When the program has started: looks once, if the settings say so.
  void start();
  /// Told by the meeting: true from "join" to the end of the meeting.
  void setInMeeting(bool inMeeting);
  Q_INVOKABLE void checkNow();
  Q_INVOKABLE void later();    // hides the notice until the next start
  Q_INVOKABLE void install();
  /// For pictures of the screens: as if `version` had been found.
  void showForPicture(const QString& version);

 signals:
  void changed();
  void closeRequested();  // the installer is about to run

 private:
  void received(bool ok, const QByteArray& body);

  AppState& app_;
  const QString currentVersion_;
  const QUrl latestUrl_;
  std::unique_ptr<backhaul::Updater> updater_;
  Fetch fetch_;
  Clock clock_;
  QString latestVersion_;
  QString notesEn_;
  QString notesJa_;
  QString status_ = QStringLiteral("idle");
  QString checkedAt_;
  bool inMeeting_ = false;
  bool saidLater_ = false;
  // Read by the updater's own thread when it asks whether the program may close.
  std::shared_ptr<std::atomic<bool>> mayClose_ = std::make_shared<std::atomic<bool>>(true);
};

/// The real fetch: HTTPS, follows redirects, gives up after 15 seconds or 64 KiB.
UpdateController::Fetch makeNetworkFetch(QObject* owner);
