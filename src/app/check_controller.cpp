#include "check_controller.h"

#include <QTime>
#include <QVariantMap>

#include "app_state.h"
#include "audio_controller.h"
#include "dictionary_translator.h"
#include "licence_controller.h"
#include "meeting_controller.h"
#include "update_controller.h"

using backhaul::CheckId;
using backhaul::CheckItem;
using backhaul::CheckLevel;
using backhaul::CheckReason;
using backhaul::ShowFacts;

CheckController::CheckController(AppState& app, MeetingController& meeting, LicenceController* licence,
                                 AudioController* audio, UpdateController* updates, System system, Clock clock,
                                 QObject* parent)
    : QObject(parent),
      app_(app),
      meeting_(meeting),
      licence_(licence),
      audio_(audio),
      updates_(updates),
      system_(system ? std::move(system) : [] { return readSystemFacts(); }),
      clock_(clock ? std::move(clock) : [] { return QTime::currentTime().toString(QStringLiteral("HH:mm")); }) {
  // The words follow the language. Queued: the texts are made here, so this has to run
  // after the program has switched its language.
  connect(&app_, &AppState::languageChanged, this, [this] {
    if (sample_) {
      showForPicture();
    } else if (!checkedAt_.isEmpty()) {
      const QString at = checkedAt_;
      show(gather());
      checkedAt_ = at;  // the same look, in other words
      emit changed();
    }
  }, Qt::QueuedConnection);
}

int CheckController::count(CheckLevel level) const {
  int n = 0;
  for (const CheckItem& item : judged_) n += item.level == level ? 1 : 0;
  return n;
}

ShowFacts CheckController::gather() const {
  ShowFacts f;
  f.playsAudio = app_.playsAudio();
  f.sendsAudio = app_.sendsAudio();
  f.showsVideo = app_.receivesVideo();
  f.netAsked = netAsked_;
  f.netReachable = netReachable_;
  f.netMs = netMs_;
  f.ownKeys = meeting_.credentialsSaved();
  if (licence_ != nullptr) {
    f.licenceActive = licence_->active();
    f.licenceStopped = licence_->stopped();
    f.licenceDaysLeft = licence_->daysLeft();
    f.zoomSignedIn = licence_->zoomSignedIn();
  }
  f.meetingIdGood = MeetingController::parseMeetingNumber(app_.meetingId()) != 0;
  f.outputChosen = !app_.property("audioDeviceName").toString().isEmpty();
  f.outputRunning = audio_ != nullptr && audio_->deviceIndex() > 0;
  f.inputChosen = !app_.micDeviceName().isEmpty();
  f.inputRunning = audio_ != nullptr && audio_->micIndex() > 0;
  app_.outputMonitors(&f.monitorsChosen, &f.monitorsMissing);
  const SystemFacts system = system_();
  f.power = system.power;
  f.notifications = system.notifications;
  if (updates_ != nullptr) {
    f.update = updates_->status() == QStringLiteral("current") ? 1 : updates_->status() == QStringLiteral("available") ? 2 : 0;
  }
  return f;
}

void CheckController::run() {
  sample_ = false;
  const int asked = ++round_;
  netAsked_ = false;
  show(gather());  // what is known at once
  if (licence_ == nullptr || !licence_->available()) return;
  running_ = true;
  emit changed();
  licence_->probe([this, asked](bool reachable, int ms) {
    if (asked != round_) return;  // a newer look is under way
    running_ = false;
    netAsked_ = true;
    netReachable_ = reachable;
    netMs_ = ms;
    show(gather());
  });
}

void CheckController::show(const ShowFacts& facts) {
  judged_ = backhaul::checkBeforeShow(facts);
  items_.clear();
  for (const CheckItem& item : judged_) {
    QVariantMap line;
    line[QStringLiteral("name")] = nameOf(item.id);
    line[QStringLiteral("text")] = textOf(item, facts);
    line[QStringLiteral("level")] = item.level == CheckLevel::Ok     ? QStringLiteral("ok")
                                    : item.level == CheckLevel::Warn ? QStringLiteral("warn")
                                                                     : QStringLiteral("bad");
    items_.push_back(line);
  }
  checkedAt_ = clock_();
  emit changed();
}

QString CheckController::nameOf(CheckId id) const {
  switch (id) {
    case CheckId::Internet: return T("インターネット");
    case CheckId::Licence: return T("ライセンス");
    case CheckId::ZoomSignIn: return T("Zoomのサインイン");
    case CheckId::MeetingId: return T("会議ID");
    case CheckId::AudioOutput: return T("音声の機器");
    case CheckId::AudioInput: return T("入力の機器");
    case CheckId::VideoOutputs: return T("映像の出力先");
    case CheckId::Power: return T("電源とスリープ");
    case CheckId::Notifications: return T("Windowsの通知");
    case CheckId::Update: return T("更新");
  }
  return QString();
}

QString CheckController::textOf(const CheckItem& item, const ShowFacts& f) const {
  const QString awake = canKeepAwake() ? T("会議中はスリープと画面オフを止めます") : QString();
  switch (item.reason) {
    case CheckReason::Reachable: return T("ライセンスのサーバーまで %1ms").arg(f.netMs);
    case CheckReason::Slow: return T("ライセンスのサーバーまで %1ms（遅めです。回線を確かめてください）").arg(f.netMs);
    case CheckReason::Unreachable: return T("ライセンスのサーバーに届きません。ネットを確かめてください");
    case CheckReason::LicenceGood:
      return licence_ == nullptr ? QString()
                                 : T("PC %1 / %2台・%3").arg(licence_->used()).arg(licence_->seats()).arg(licence_->expiresText());
    case CheckReason::LicenceEndsSoon: return T("あと%1日で期限が切れます（%2）").arg(f.licenceDaysLeft).arg(licence_ != nullptr ? licence_->expiresText() : QString());
    case CheckReason::LicenceStopped: return T("このライセンスは使えません（期限切れか、止められています）");
    case CheckReason::NoLicence: return T("ライセンスキーが入っていません");
    case CheckReason::OwnKeys: return T("このPCに入れたZoomアプリの鍵で入ります");
    case CheckReason::SignedIn: return T("サインイン済み（ほかのアカウントの会議にも入れます）");
    case CheckReason::NotSignedIn: return T("サインインしていません（自分のアカウントの会議だけなら不要です）");
    case CheckReason::IdGood: return app_.meetingId().trimmed();
    case CheckReason::IdMissing: return T("会議IDが入っていないか、形が違います");
    case CheckReason::DeviceRunning:
      return item.id == CheckId::AudioInput ? app_.micDeviceName()
                                            : T("%1・出力 %2ch").arg(app_.property("audioDeviceName").toString()).arg(app_.property("audioChannels").toInt());
    case CheckReason::DeviceNotRunning:
      return T("「%1」が動いていません（探しています）").arg(item.id == CheckId::AudioInput ? app_.micDeviceName() : app_.property("audioDeviceName").toString());
    case CheckReason::DeviceNotChosen:
      return item.id == CheckId::AudioInput ? T("選ばれていません（Zoomへ音を送りません）") : T("選ばれていません（会議の音が出ません）");
    case CheckReason::OutputsGood: return T("モニター %1台に出します").arg(f.monitorsChosen);
    case CheckReason::OutputMissing: return T("出力先のモニターが %1台つながっていません").arg(f.monitorsMissing);
    case CheckReason::NoOutputChosen: return T("出力先のモニターが決まっていません");
    case CheckReason::OnMains: return awake.isEmpty() ? T("電源につながっています") : T("電源につながっています。") + awake;
    case CheckReason::OnBattery: return awake.isEmpty() ? T("バッテリーで動いています。電源をつないでください") : T("バッテリーで動いています。電源をつないでください。") + awake;
    case CheckReason::NotificationsHeld: return T("いまは通知が出ない状態です");
    case CheckReason::NotificationsShown: return T("通知が出る状態です。Windowsの「応答不可」をオンにしてください");
    case CheckReason::UpToDate: return updates_ != nullptr ? T("最新です（%1）").arg(updates_->currentVersion()) : QString();
    case CheckReason::NewerWaiting: return updates_ != nullptr ? T("バージョン %1 があります。本番のあとに更新してください").arg(updates_->latestVersion()) : QString();
  }
  return QString();
}

void CheckController::showForPicture() {
  sample_ = true;
  ShowFacts f;
  f.playsAudio = f.showsVideo = true;
  f.netAsked = f.netReachable = true;
  f.netMs = 41;
  f.licenceActive = true;
  f.licenceDaysLeft = 300;
  f.zoomSignedIn = true;
  f.meetingIdGood = true;
  f.outputChosen = f.outputRunning = true;
  f.monitorsChosen = 3;
  f.monitorsMissing = 1;
  f.power = 1;
  f.notifications = 1;
  f.update = 1;
  judged_ = backhaul::checkBeforeShow(f);
  items_.clear();
  const QStringList texts{T("ライセンスのサーバーまで %1ms").arg(41), T("PC %1 / %2台・%3").arg(2).arg(5).arg(QStringLiteral("2027-11-30") + T(" まで")),
                          T("サインイン済み（ほかのアカウントの会議にも入れます）"), QStringLiteral("123 4567 8901"),
                          T("%1・出力 %2ch").arg(QStringLiteral("Sample audio device")).arg(8),
                          T("出力先のモニターが %1台つながっていません").arg(1),
                          T("電源につながっています。") + T("会議中はスリープと画面オフを止めます"),
                          T("通知が出る状態です。Windowsの「応答不可」をオンにしてください"), T("最新です（%1）").arg(QStringLiteral("0.4.0"))};
  for (std::size_t i = 0; i < judged_.size(); ++i) {
    QVariantMap line;
    line[QStringLiteral("name")] = nameOf(judged_[i].id);
    line[QStringLiteral("text")] = static_cast<qsizetype>(i) < texts.size() ? texts[static_cast<qsizetype>(i)] : QString();
    line[QStringLiteral("level")] = judged_[i].level == CheckLevel::Ok     ? QStringLiteral("ok")
                                    : judged_[i].level == CheckLevel::Warn ? QStringLiteral("warn")
                                                                           : QStringLiteral("bad");
    items_.push_back(line);
  }
  checkedAt_ = QStringLiteral("14:02");
  emit changed();
}
