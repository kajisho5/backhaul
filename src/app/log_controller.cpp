#include "log_controller.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QFileInfo>
#include <QSaveFile>

#include "app_state.h"
#include "audio_controller.h"
#include "dictionary_translator.h"
#include "file_pick.h"
#include "meeting_controller.h"
#include "participant_model.h"

using backhaul::LogKind;

namespace {

QString wordOf(LogKind kind) {
  switch (kind) {
    case LogKind::MeetingJoined: return T("会議に入りました");
    case LogKind::MeetingLeft: return T("会議から出ました");
    case LogKind::MeetingDropped: return T("会議から切れました（自動で入り直します）");
    case LogKind::PersonHere: return T("すでに参加していました");
    case LogKind::PersonCame: return T("参加しました");
    case LogKind::PersonLeft: return T("退出しました");
    case LogKind::AudioLost: return T("音声の機器が止まりました");
    case LogKind::AudioBack: return T("音声の機器が戻りました");
    case LogKind::AudioOnSpare: return T("予備の音声機器に切り替わりました");
    case LogKind::Restarted: return T("異常終了のあと、起動し直しました");
  }
  return {};
}

}  // namespace

LogController::LogController(AppState& app, MeetingController& meeting, AudioController* audio, Clock clock, QObject* parent)
    : QObject(parent), app_(app), meeting_(meeting), audio_(audio), clock_(std::move(clock)) {
  connect(&app_, &AppState::changed, this, [this] { lookAtMeeting(); });
  connect(&meeting_, &MeetingController::changed, this, [this] { lookAtMeeting(); });
  const QAbstractItemModel* people = app_.participants();
  connect(people, &QAbstractItemModel::modelReset, this, [this] { lookAtPeople(); });
  connect(people, &QAbstractItemModel::rowsInserted, this, [this] { lookAtPeople(); });
  connect(people, &QAbstractItemModel::rowsRemoved, this, [this] { lookAtPeople(); });
  if (audio_ != nullptr) {
    connect(audio_, &AudioController::changed, this, [this] { lookAtAudio(); });
    audioLost_ = audio_->outputSeeking();
    onSpare_ = audio_->onSpare();
  }
  lookAtMeeting();  // already in a meeting when the log starts (the demo is)
}

qint64 LogController::nowMs() const { return clock_ ? clock_() : QDateTime::currentMSecsSinceEpoch(); }

bool LogController::canSave() const { return canPickFiles(); }

void LogController::note(LogKind kind, const QString& who) {
  log_.add(nowMs(), kind, who.toStdString());
  emit changed();
}

void LogController::lookAtMeeting() {
  const bool in = app_.connected_;
  if (in == wasIn_) return;
  wasIn_ = in;
  if (in) {
    note(LogKind::MeetingJoined, app_.meetingName_);
    // The list may have come before the meeting counted as joined. Not after a drop: the
    // list on the screen is then still the old one, kept so that people return to their
    // slots; the new one follows.
    if (!afterDrop_) lookAtPeople();
    afterDrop_ = false;
    return;
  }
  // Who was there is forgotten without a line each: this PC went, not they.
  people_.clear();
  listed_ = false;
  afterDrop_ = meeting_.rejoining();
  note(afterDrop_ ? LogKind::MeetingDropped : LogKind::MeetingLeft);
}

void LogController::lookAtPeople() {
  if (!wasIn_) return;
  const QVector<ParticipantRow>& rows = app_.participants()->rows();
  if (rows.isEmpty()) return;  // the list is not there yet (or being put away)
  QHash<quint32, QString> now;
  for (const ParticipantRow& row : rows) {
    if (!row.isSelf) now.insert(row.id, row.name);
  }
  for (const ParticipantRow& row : rows) {  // in the order of the list
    if (row.isSelf || people_.contains(row.id)) continue;
    note(listed_ ? LogKind::PersonCame : LogKind::PersonHere, row.name);
  }
  for (auto it = people_.constBegin(); it != people_.constEnd(); ++it) {
    if (!now.contains(it.key())) note(LogKind::PersonLeft, it.value());
  }
  people_ = now;
  listed_ = true;
}

void LogController::lookAtAudio() {
  const bool lost = audio_->outputSeeking(), spare = audio_->onSpare();
  if (spare && !onSpare_) {
    note(LogKind::AudioOnSpare, app_.spareDeviceName());
  } else if (lost && !audioLost_) {
    note(LogKind::AudioLost, app_.audioDeviceName_);
  } else if (!lost && !spare && (audioLost_ || onSpare_)) {
    note(LogKind::AudioBack, app_.audioDeviceName_);
  }
  audioLost_ = lost;
  onSpare_ = spare;
}

QByteArray LogController::csv() const {
  const std::string text = log_.csv(
      T("時刻,できごと,名前").toStdString(),
      [](std::int64_t at) { return QDateTime::fromMSecsSinceEpoch(at).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")).toStdString(); },
      [](LogKind kind) { return wordOf(kind).toStdString(); });
  return QByteArray("\xEF\xBB\xBF") + QByteArray::fromStdString(text);
}

void LogController::save() {
  const QString suggested = QStringLiteral("backhaul-log-%1.csv").arg(QDateTime::fromMSecsSinceEpoch(nowMs()).toString(QStringLiteral("yyyyMMdd-HHmm")));
  const QString path = pickSaveFile(T("本番ログを書き出す"), suggested, FileKind::Csv);
  if (!path.isEmpty()) saveTo(path);
}

bool LogController::saveTo(const QString& path) {
  QSaveFile file(path);  // an old file of that name stays whole unless the new one is written completely
  const QByteArray text = csv();
  failed_ = !file.open(QIODevice::WriteOnly) || file.write(text) != text.size() || !file.commit();
  message_ = failed_ ? T("書き出せませんでした：%1").arg(file.errorString()) : T("「%1」に書き出しました（%2件）").arg(QFileInfo(path).fileName()).arg(count());
  emit changed();
  return !failed_;
}
