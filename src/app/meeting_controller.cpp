#include "meeting_controller.h"
#include "dictionary_translator.h"

#include <QDateTime>
#include <algorithm>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>

#include "app_state.h"
#include "licence_controller.h"
#include "core/audio_mixer.h"
#include "core/quality_policy.h"
#include "core/recovery.h"
#include "core/yuv.h"
#include "frame_store.h"

using backhaul::MeetingState;

namespace {

backhaul::VideoSize toVideoSize(backhaul::Resolution r) {
  switch (r) {
    case backhaul::Resolution::R1080: return backhaul::VideoSize::P1080;
    case backhaul::Resolution::R720: return backhaul::VideoSize::P720;
    case backhaul::Resolution::R360: return backhaul::VideoSize::P360;
    case backhaul::Resolution::R180: return backhaul::VideoSize::P180;
    case backhaul::Resolution::R90: break;
  }
  return backhaul::VideoSize::P90;
}

bool isPerson(const SlotRow& r) { return r.situation.kind == backhaul::SlotKind::Person; }

}  // namespace

MeetingController::MeetingController(AppState& app, FrameStore& frames, std::unique_ptr<backhaul::MeetingClient> client,
                                     std::unique_ptr<CredentialStore> credentials, QObject* parent)
    : QObject(parent), app_(app), frames_(frames), client_(std::move(client)), credentials_(std::move(credentials)) {
  if (credentials_ != nullptr) {
    ZoomCredentials saved;
    if (credentials_->load(saved)) {
      clientId_ = saved.clientId;
      clientSecret_ = saved.clientSecret;
    }
  }
  pollTimer_.setInterval(400);
  connect(&pollTimer_, &QTimer::timeout, this, [this] {
    if (client_ != nullptr) client_->poll();
  });
  rejoinTimer_.setSingleShot(true);
  connect(&rejoinTimer_, &QTimer::timeout, this, [this] { rejoinNow(); });
  secondTimer_.setInterval(1000);
  connect(&secondTimer_, &QTimer::timeout, this, [this] { everySecond(); });
  connect(&app_, &AppState::assignmentsChanged, this, [this] { applyParticipants(); });
  audio_ = app_.playsAudio();
  connect(&app_, &AppState::roleChanged, this, [this] {
    audio_ = app_.playsAudio();
    applyParticipants();  // the role decides which slots are in use
    applyShares();
  });
  connect(&app_, &AppState::changed, this, [this] {
    emit changed();  // canJoin depends on what is typed
    updateWants();   // the size limit may have changed
  });
}

MeetingController::~MeetingController() {
  // Stop the client first: it calls back into this object.
  client_.reset();
}

QString MeetingController::credentialHint() const {
  if (clientId_.size() <= 8) return clientId_.isEmpty() ? QString() : QStringLiteral("…");
  return clientId_.left(4) + QStringLiteral("…") + clientId_.right(4);
}

int MeetingController::step() const {
  switch (state_) {
    case MeetingState::Receiving: return 3;
    case MeetingState::WaitingForPermission: return 2;
    case MeetingState::Joining: return 1;
    default: return 0;
  }
}

qint64 MeetingController::nowMs() const { return clock_ ? clock_() : QDateTime::currentMSecsSinceEpoch(); }

QString MeetingController::line() const {
  switch (line_) {
    case backhaul::LineGrade::Good: return QStringLiteral("good");
    case backhaul::LineGrade::Fair: return QStringLiteral("fair");
    case backhaul::LineGrade::Poor: return QStringLiteral("poor");
    case backhaul::LineGrade::Unknown: break;
  }
  return QString();
}

// A slot's person is shown as unstable once their line has been poor for a while.
void MeetingController::applyUnstable() {
  const qint64 now = nowMs();
  const auto& slotRows = app_.slotList()->rows();
  for (int i = 0; i < slotRows.size(); ++i) {
    const SlotRow& r = slotRows[i];
    if (!isPerson(r)) continue;
    const bool unstable = r.userId != 0 && r.situation.connected && lineWatch_.unstable(r.userId, now);
    if (r.situation.unstable != unstable) {
      app_.slotList()->update(i, [unstable](SlotRow& row) { row.situation.unstable = unstable; });
    }
  }
}

void MeetingController::refreshLine() {
  if (client_ != nullptr && app_.connected_) {
    const backhaul::LineQuality quality = client_->lineQuality();
    const backhaul::LineGrade line =
        backhaul::worse(backhaul::gradeOfStep(quality.receive), backhaul::gradeOfStep(quality.send));
    if (line != line_) {
      line_ = line;
      emit changed();
    }
  }
  applyUnstable();  // a poor line becomes "unstable" with time, not with a new message
}

int MeetingController::rejoinWaitSeconds() const {
  if (!rejoining() || rejoinNextMs_ == 0) return 0;
  return static_cast<int>(std::max<qint64>(0, (rejoinNextMs_ - nowMs() + 999) / 1000));
}

bool MeetingController::busy() const {
  // Coming back counts: the operator's wish to be in the meeting still stands.
  return rejoining() || preparing_ || state_ == MeetingState::Authenticating || state_ == MeetingState::Joining ||
         state_ == MeetingState::WaitingForPermission || state_ == MeetingState::Receiving;
}

QString MeetingController::statusText() const {
  if (preparing_) return T("入場券を取っています…");
  if (rejoining() && (state_ == MeetingState::Failed || state_ == MeetingState::Ended)) return T("入り直し中");
  switch (state_) {
    case MeetingState::Idle: return T("未接続");
    case MeetingState::Authenticating: return T("鍵を確認中");
    case MeetingState::Joining: return T("参加しています…");
    case MeetingState::WaitingForPermission: return T("参加中（許可待ち）");
    case MeetingState::Receiving: return T("参加中");
    case MeetingState::Failed: return T("失敗");
    case MeetingState::Ended: return T("会議が終わりました");
  }
  return QString();
}

quint64 MeetingController::parseMeetingNumber(const QString& text) {
  QString digits;
  for (const QChar c : text) {
    if (c.isDigit() && c.unicode() < 128) {
      digits.append(c);
    } else if (c != QLatin1Char(' ') && c != QLatin1Char('-')) {
      return 0;
    }
  }
  if (digits.size() < 9 || digits.size() > 11) return 0;
  return digits.toULongLong();
}

bool MeetingController::canJoin() const {
  const bool keys = credentialsSaved() || (licence_ != nullptr && licence_->active());
  return client_ != nullptr && !busy() && keys && parseMeetingNumber(app_.meetingId()) != 0 &&
         !app_.joinName().trimmed().isEmpty();
}

void MeetingController::join() {
  if (!canJoin()) return;
  stopRejoining();
  wasIn_ = false;
  startJoin();
}

void MeetingController::forgetResume() {
  if (resumeStore_ != nullptr) resumeStore_->clear();
  resumeKept_ = false;
}

bool MeetingController::resume() {
  if (resumeStore_ == nullptr) return false;
  const QByteArray kept = resumeStore_->load();
  if (kept.isEmpty()) return false;
  const QJsonObject o = QJsonDocument::fromJson(kept).object();
  passcode_ = o.value(QStringLiteral("passcode")).toString().left(64);
  zak_ = o.value(QStringLiteral("zak")).toString().left(4096);
  emit changed();
  if (!canJoin()) {
    forgetResume();
    return false;
  }
  join();
  // Like a meeting this PC was thrown out of: should this try fail for the line, the next ones follow.
  wasIn_ = true;
  return true;
}

void MeetingController::stopRejoining() {
  rejoinTimer_.stop();
  rejoinAttempt_ = 0;
  rejoinSinceMs_ = rejoinNextMs_ = 0;
}

// Decides, at the end of a meeting or of a try, whether this PC comes back by itself.
// True when the next try is set up.
bool MeetingController::planRejoin() {
  const backhaul::EndCause cause = endCause_;
  endCause_ = backhaul::EndCause::None;
  const bool had = wasIn_ || rejoining();
  wasIn_ = false;
  if (client_ == nullptr || !had || !app_.rejoins() || !backhaul::shouldRejoin(cause)) {
    stopRejoining();
    return false;
  }
  const qint64 now = nowMs();
  if (!rejoining()) rejoinSinceMs_ = now;
  const qint64 wait = qint64{1000} * backhaul::rejoinWaitSeconds(rejoinAttempt_ + 1);
  if (now + wait - rejoinSinceMs_ > qint64{1000} * backhaul::kRejoinGiveUpSeconds) {
    stopRejoining();
    detail_ = T("10分たっても会議に入り直せませんでした。回線を確かめて、もう一度「参加」を押してください");
    return false;
  }
  ++rejoinAttempt_;
  rejoinNextMs_ = now + wait;
  rejoinTimer_.start(static_cast<int>(wait));
  return true;
}

void MeetingController::rejoinNow() {
  if (!rejoining() || rejoinNextMs_ == 0) return;  // not waiting for a try
  rejoinTimer_.stop();
  rejoinNextMs_ = 0;
  startJoin();
}

// The join itself, for the operator's "join" and for coming back alike.
void MeetingController::startJoin() {
  backhaul::JoinRequest request;
  request.clientId = clientId_.toStdString();
  request.clientSecret = clientSecret_.toStdString();
  request.meetingNumber = parseMeetingNumber(app_.meetingId());
  request.displayName = app_.joinName().trimmed().toStdString();
  request.passcode = passcode_.toStdString();
  request.zak = zak_.trimmed().toStdString();
  request.sendAudio = app_.sendsAudio() && !app_.micDeviceName().isEmpty();
  // Joining the sound opens it in both directions, so only a PC that plays the voices or
  // has a device of its own to send from joins it.
  request.audio = app_.playsAudio() || request.sendAudio;
  request.sendQuality = app_.sendQuality() == 2   ? backhaul::SendQuality::Music
                        : app_.sendQuality() == 1 ? backhaul::SendQuality::Original
                                                  : backhaul::SendQuality::Speech;
  request.sendStereo = app_.sendStereo();
  request.sendVideo = !app_.sendsVideo() || app_.videoDeviceName().isEmpty() ? backhaul::SendVideo::None
                      : app_.returnAsShare()                                  ? backhaul::SendVideo::Share
                                                                              : backhaul::SendVideo::Camera;
  request.receive = app_.receivesAnything();
  authenticated_ = false;
  detail_.clear();
  joinNotice_.clear();
  // Zoom keys typed into this PC come first: that is the operator's explicit choice.
  if (credentialsSaved()) {
    client_->join(request, this);  // reports through onMeetingState, also when it cannot start
    return;
  }
  request.clientId.clear();
  request.clientSecret.clear();
  preparing_ = true;
  const int asked = ++prepareRound_;
  emit changed();
  licence_->prepareJoin([this, request, asked](const LicenceController::Entry& entry) mutable {
    if (asked != prepareRound_) return;  // the operator pressed "leave" while this was on its way
    preparing_ = false;
    if (!entry.ok) {
      // The licence server out of reach is the line again; a refusal is not.
      endCause_ = entry.unreachable ? backhaul::EndCause::Connection : backhaul::EndCause::Other;
      onMeetingState(MeetingState::Failed, {});
      if (detail_.isEmpty()) detail_ = entry.error;  // unless giving up had something to say
      emit changed();
      return;
    }
    request.sdkToken = entry.sdkToken.toStdString();
    // The ZAK from the sign-in; one typed by hand is the fallback for when there is none.
    if (!entry.zak.isEmpty()) request.zak = entry.zak.toStdString();
    joinNotice_ = entry.notice;
    emit changed();
    client_->join(request, this);
  });
}

void MeetingController::setLicence(LicenceController* licence) {
  licence_ = licence;
  if (licence_ != nullptr) connect(licence_, &LicenceController::changed, this, [this] { emit changed(); });
  emit changed();
}

bool MeetingController::sendAudio(const std::int16_t* samples, std::size_t frames, int channels) {
  // client_ is set once in the constructor and outlives every caller.
  return client_ != nullptr && client_->sendAudio(samples, frames, channels);
}

bool MeetingController::sendVideo(const std::uint8_t* i420, int width, int height) {
  return client_ != nullptr && client_->sendVideo(i420, width, height);
}

void MeetingController::leave() {
  const bool waiting = rejoining() && rejoinNextMs_ != 0;  // between two tries: nothing is running
  stopRejoining();
  wasIn_ = false;
  if (preparing_ || waiting) {
    // Nothing is started at the meeting service: an answer on its way is simply not used.
    ++prepareRound_;
    preparing_ = false;
    onMeetingState(MeetingState::Idle, {});
    return;
  }
  if (client_ != nullptr && busy()) client_->leave();
}

bool MeetingController::saveCredentials(const QString& clientId, const QString& clientSecret) {
  const QString id = clientId.trimmed();
  const QString secret = clientSecret.trimmed();
  if (credentials_ == nullptr || id.isEmpty() || secret.isEmpty()) return false;
  if (!credentials_->save({id, secret})) return false;
  clientId_ = id;
  clientSecret_ = secret;
  emit changed();
  return true;
}

void MeetingController::clearCredentials() {
  if (credentials_ == nullptr || !credentials_->clear()) return;
  clientId_.clear();
  clientSecret_.clear();
  emit changed();
}

void MeetingController::onMeetingState(MeetingState state, const std::string& detail) {
  state_ = state;
  detail_ = T(detail);  // said in Japanese by the Zoom part
  if (state == MeetingState::Joining) authenticated_ = true;
  const bool in = state == MeetingState::WaitingForPermission || state == MeetingState::Receiving;
  const bool over = state == MeetingState::Failed || state == MeetingState::Ended;
  if (in) {
    wasIn_ = true;
    stopRejoining();  // back in
    if (resumeStore_ != nullptr && !resumeKept_) {
      QJsonObject kept;
      kept[QStringLiteral("passcode")] = passcode_;
      kept[QStringLiteral("zak")] = zak_;
      resumeKept_ = resumeStore_->save(QJsonDocument(kept).toJson(QJsonDocument::Compact));
    }
  }
  if (state == MeetingState::Idle) {
    endCause_ = backhaul::EndCause::None;
    wasIn_ = false;
  }
  if (over && planRejoin()) {
    // Coming back: the slots stay as they are (their people show as lost and return to
    // them by name); only what belongs to the connection that just ended is put away.
    sendingAudio_ = sendingVideo_ = false;
    sendNote_.clear();
    sendVideoNote_.clear();
    app_.elapsed_.clear();
    joinedAtMs_ = 0;
  }
  const bool live = busy();
  if (live && !pollTimer_.isActive()) {
    pollTimer_.start();
    secondTimer_.start();
  }
  if (!in) line_ = backhaul::LineGrade::Unknown;
  if (!live) {
    forgetResume();  // out of the meeting for good: nothing to come back to
    lineWatch_.clear();
    sendingAudio_ = sendingVideo_ = false;
    sendNote_.clear();
    sendVideoNote_.clear();
    pollTimer_.stop();
    secondTimer_.stop();
    everyone_.clear();
    shares_.clear();
    shareSource_ = 0;
    shareSlot_ = -1;
    resetSlots();
    app_.meetingName_.clear();
    app_.elapsed_.clear();
    joinedAtMs_ = 0;
  }
  if ((state == MeetingState::WaitingForPermission || state == MeetingState::Receiving) && joinedAtMs_ == 0) {
    joinedAtMs_ = QDateTime::currentMSecsSinceEpoch();
  }
  app_.connected_ = state == MeetingState::WaitingForPermission || state == MeetingState::Receiving;
  app_.notifyChanged();
  emit changed();
}

void MeetingController::onMeetingTopic(const std::string& topic) {
  app_.meetingName_ = QString::fromStdString(topic);
  app_.notifyChanged();
}

void MeetingController::onParticipants(const std::vector<backhaul::Participant>& everyone) {
  everyone_ = everyone;
  applyParticipants();
}

void MeetingController::onShares(const std::vector<backhaul::ShareSource>& shares) {
  shares_ = shares;
  applyShares();
}

// Shows one shared screen in the slide slot: the one already showing while it lasts,
// otherwise the first one there is. When sharing stops the slot keeps the last picture
// or goes black, as set for the slot.
void MeetingController::applyShares() {
  if (!busy()) return;
  const int row = app_.shareRow();
  const bool used = row >= 0 && app_.slotActive(row);
  std::uint32_t source = 0, sharer = 0;
  if (used) {
    for (const backhaul::ShareSource& share : shares_) {
      if (share.sourceId == shareSource_.load() || source == 0) {
        source = share.sourceId;
        sharer = share.userId;
      }
      if (share.sourceId == shareSource_.load()) break;
    }
  }
  if (row >= 0) {
    const SlotRow& now = app_.slotList()->rows()[row];
    if (source != 0) {
      QString name;
      for (const backhaul::Participant& p : everyone_) {
        if (p.id == sharer) name = QString::fromStdString(p.name);
      }
      const QString text = name.isEmpty() ? T("共有中") : T("共有：%1").arg(name);
      if (!now.situation.assigned || !now.situation.connected || !now.situation.videoOn || now.zoomName != text) {
        app_.slotList()->update(row, [&text](SlotRow& r) {
          r.zoomName = text;
          r.situation.assigned = r.situation.connected = r.situation.videoOn = true;
        });
      }
    } else if (!used) {
      if (now.situation.assigned) {
        app_.slotList()->update(row, [](SlotRow& r) {
          r.zoomName.clear();
          r.situation.assigned = r.situation.connected = r.situation.videoOn = r.situation.hasLastFrame = false;
          r.width = r.height = r.fps = 0;
        });
        frames_.clear(row);
      }
    } else if (now.situation.assigned && now.situation.connected) {
      app_.slotList()->update(row, [](SlotRow& r) {  // sharing stopped
        r.situation.connected = r.situation.videoOn = false;
        r.fps = 0;
      });
    }
  }
  shareSlot_ = source != 0 ? row : -1;
  shareSource_ = source;
  if (client_ != nullptr) client_->setShareWant(source);
  app_.notifyChanged();
}

void MeetingController::resetSlots() {
  const int count = static_cast<int>(app_.slotList()->rows().size());
  for (int i = 0; i < count; ++i) {
    app_.slotList()->update(i, [](SlotRow& r) {
      r.userId = 0;
      r.situation.assigned = r.situation.connected = false;
      r.situation.videoOn = r.situation.audioOn = r.situation.hasLastFrame = r.situation.unstable = false;
      r.width = r.height = r.fps = 0;
      r.level = 0.0;
      // "Shared by ..." belongs to the meeting that just ended, not to the slot.
      if (r.situation.kind == backhaul::SlotKind::Share) r.zoomName.clear();
    });
  }
  frames_.clearAll();
  if (mixer_ != nullptr) {
    for (int i = 0; i < mixer_->sources(); ++i) mixer_->flush(i);
  }
  app_.participants()->setRows({}, app_.slotNumbers());
  {
    const QMutexLocker lock(&routeMutex_);
    slotByUser_.clear();
  }
  wants_.clear();
  if (client_ != nullptr) client_->setVideoWants({});
}

void MeetingController::applyParticipants() {
  if (!busy()) return;
  const auto& slotRows = app_.slotList()->rows();
  QVector<ParticipantRow> people;
  std::vector<std::uint32_t> here;
  for (const backhaul::Participant& p : everyone_) here.push_back(p.id);
  lineWatch_.keepOnly(here);  // who left is forgotten: a number may be given to somebody else
  for (const backhaul::Participant& p : everyone_) {
    lineWatch_.report(p.id, backhaul::gradeOfStep(p.lineStep) == backhaul::LineGrade::Poor, nowMs());
    ParticipantRow row;
    row.id = p.id;
    row.name = QString::fromStdString(p.name);
    row.isSelf = p.isSelf;
    row.role = p.isSelf ? T("自分") : p.isHost ? T("主催") : p.isCoHost ? T("共同") : QString();
    people.push_back(row);
  }

  QHash<quint32, int> slotByUser;
  QVector<int> personOfSlot(slotRows.size(), -1);
  // Slots the current role does not use hold nobody.
  for (int i = 0; i < slotRows.size(); ++i) {
    if (!isPerson(slotRows[i]) || app_.slotActive(i)) continue;
    if (slotRows[i].userId == 0 && !slotRows[i].situation.assigned) continue;
    app_.slotList()->update(i, [](SlotRow& r) {
      r.userId = 0;
      r.situation.assigned = r.situation.connected = false;
      r.situation.videoOn = r.situation.audioOn = r.situation.hasLastFrame = r.situation.unstable = false;
      r.width = r.height = r.fps = 0;
      r.level = 0.0;
    });
    frames_.clear(i);
    if (mixer_ != nullptr) mixer_->flush(i);
  }
  // 1) People who are already in a slot stay there.
  for (int i = 0; i < slotRows.size(); ++i) {
    if (!isPerson(slotRows[i]) || !app_.slotActive(i) || slotRows[i].userId == 0) continue;
    for (int k = 0; k < people.size(); ++k) {
      if (people[k].id == slotRows[i].userId && people[k].slot < 0) {
        people[k].slot = i;
        personOfSlot[i] = k;
        break;
      }
    }
  }
  // 2) Empty slots take the person whose Zoom name they are set up for (first join and return).
  for (int i = 0; i < slotRows.size(); ++i) {
    if (!isPerson(slotRows[i]) || !app_.slotActive(i) || personOfSlot[i] >= 0 ||
        slotRows[i].zoomName.trimmed().isEmpty()) {
      continue;
    }
    for (int k = 0; k < people.size(); ++k) {
      if (people[k].slot < 0 && !people[k].isSelf && people[k].name == slotRows[i].zoomName.trimmed()) {
        people[k].slot = i;
        personOfSlot[i] = k;
        break;
      }
    }
  }
  for (int i = 0; i < slotRows.size(); ++i) {
    if (!isPerson(slotRows[i]) || !app_.slotActive(i)) continue;
    const int k = personOfSlot[i];
    if (k >= 0) {
      const backhaul::Participant& p = everyone_[static_cast<std::size_t>(k)];
      slotByUser.insert(p.id, i);
      const SlotRow& now = slotRows[i];
      if (now.userId != p.id || !now.situation.assigned || !now.situation.connected ||
          now.situation.videoOn != p.videoOn || now.situation.audioOn != !p.audioMuted) {
        app_.slotList()->update(i, [&p](SlotRow& r) {
          r.userId = p.id;
          r.situation.assigned = r.situation.connected = true;
          r.situation.videoOn = p.videoOn;
          r.situation.audioOn = !p.audioMuted;
        });
      }
    } else if (slotRows[i].situation.assigned && slotRows[i].situation.connected) {
      // The person left: keep the slot theirs (by name) and show it as lost.
      app_.slotList()->update(i, [](SlotRow& r) {
        r.userId = 0;
        r.situation.connected = false;
        r.fps = 0;
        r.level = 0.0;
      });
      if (mixer_ != nullptr) mixer_->flush(i);
    }
  }
  app_.participants()->setRows(people, app_.slotNumbers());
  {
    const QMutexLocker lock(&routeMutex_);
    slotByUser_ = slotByUser;
  }
  applyUnstable();
  app_.notifyChanged();
  updateWants();
}

void MeetingController::updateWants() {
  if (client_ == nullptr || !busy()) return;
  QHash<quint32, int> slotByUser;
  {
    const QMutexLocker lock(&routeMutex_);
    slotByUser = slotByUser_;
  }
  if (!app_.receivesVideo()) slotByUser.clear();  // a sound-only PC asks for no pictures
  const backhaul::QualityCap cap = app_.qualityAuto() ? backhaul::QualityCap::Auto : backhaul::QualityCap::Max720;
  const backhaul::VideoSize size =
      toVideoSize(backhaul::pickPersonResolution(static_cast<int>(slotByUser.size()), cap));
  std::vector<backhaul::VideoWant> wants;
  for (auto it = slotByUser.constBegin(); it != slotByUser.constEnd(); ++it) wants.push_back({it.key(), size});
  std::sort(wants.begin(), wants.end(), [](const auto& a, const auto& b) { return a.userId < b.userId; });
  if (wants == wants_) return;
  wants_ = wants;
  client_->setVideoWants(wants_);
}

void MeetingController::onVideoFrame(std::uint32_t userId, const backhaul::VideoFrame& frame) {
  int slot = -1;
  {
    const QMutexLocker lock(&routeMutex_);
    slot = slotByUser_.value(userId, -1);
  }
  putFrame(slot, frame);
}

void MeetingController::onShareFrame(std::uint32_t sourceId, const backhaul::VideoFrame& frame) {
  if (sourceId != 0 && sourceId == shareSource_.load()) putFrame(shareSlot_.load(), frame);
}

void MeetingController::putFrame(int slot, const backhaul::VideoFrame& frame) {
  if (slot < 0 || frame.width <= 0 || frame.height <= 0 || frame.width > 8192 || frame.height > 8192) return;
  QImage image(frame.width, frame.height, QImage::Format_RGB32);
  if (image.isNull()) return;
  if (image.bytesPerLine() == frame.width * 4) {
    backhaul::i420ToBgra(frame.y, frame.u, frame.v, frame.width, frame.height, frame.limitedRange, image.bits());
  } else {
    return;  // RGB32 rows are never padded; refuse rather than write past a row
  }
  frames_.put(slot, image);
}

void MeetingController::everySecond() {
  if (client_ != nullptr) {
    const bool sending = client_->sendingAudio();
    const QString note = T(client_->sendAudioNote());
    const bool picture = client_->sendingVideo();
    const QString pictureNote = T(client_->sendVideoNote());
    if (sending != sendingAudio_ || note != sendNote_ || picture != sendingVideo_ || pictureNote != sendVideoNote_) {
      sendingAudio_ = sending;
      sendNote_ = note;
      sendingVideo_ = picture;
      sendVideoNote_ = pictureNote;
      emit changed();
    }
  }
  if (rejoining()) emit changed();  // the seconds until the next try
  refreshLine();
  if (joinedAtMs_ != 0) {
    const qint64 s = (QDateTime::currentMSecsSinceEpoch() - joinedAtMs_) / 1000;
    app_.elapsed_ = QStringLiteral("%1:%2:%3")
                        .arg(s / 3600, 2, 10, QLatin1Char('0'))
                        .arg((s / 60) % 60, 2, 10, QLatin1Char('0'))
                        .arg(s % 60, 2, 10, QLatin1Char('0'));
    app_.notifyChanged();
  }
  const auto& slotRows = app_.slotList()->rows();
  for (int i = 0; i < slotRows.size(); ++i) {
    if (!app_.slotActive(i)) continue;  // people and the slide alike
    const int fps = frames_.takeCount(i);
    const QImage image = frames_.latest(i);
    const bool has = !image.isNull();
    // A shared screen sends pictures only when something changes, so its size stays known
    // between them; a camera that sends nothing has no size to show.
    const bool sized = has && (fps > 0 || !isPerson(slotRows[i]));
    const int w = sized ? image.width() : 0;
    const int h = sized ? image.height() : 0;
    const SlotRow& now = slotRows[i];
    if (now.fps != fps || now.width != w || now.height != h || now.situation.hasLastFrame != has) {
      app_.slotList()->update(i, [fps, w, h, has](SlotRow& r) {
        r.fps = fps;
        r.width = w;
        r.height = h;
        r.situation.hasLastFrame = has;
      });
    }
  }
}

void MeetingController::onAudio(std::uint32_t userId, const std::int16_t* samples, std::size_t frames, int sampleRate,
                                int channels) {
  if (mixer_ == nullptr || !audio_.load()) return;
  int slot = -1;
  {
    const QMutexLocker lock(&routeMutex_);
    slot = slotByUser_.value(userId, -1);
  }
  if (slot >= 0) mixer_->push(slot, samples, frames, sampleRate, channels);
}
