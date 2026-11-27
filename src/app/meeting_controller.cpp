#include "meeting_controller.h"

#include <QDateTime>
#include <algorithm>
#include <QImage>
#include <QMutexLocker>

#include "app_state.h"
#include "core/audio_mixer.h"
#include "core/quality_policy.h"
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
  secondTimer_.setInterval(1000);
  connect(&secondTimer_, &QTimer::timeout, this, [this] { everySecond(); });
  connect(&app_, &AppState::assignmentsChanged, this, [this] { applyParticipants(); });
  audio_ = app_.handlesAudio();
  connect(&app_, &AppState::roleChanged, this, [this] {
    audio_ = app_.handlesAudio();
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

bool MeetingController::busy() const {
  return state_ == MeetingState::Authenticating || state_ == MeetingState::Joining ||
         state_ == MeetingState::WaitingForPermission || state_ == MeetingState::Receiving;
}

QString MeetingController::statusText() const {
  switch (state_) {
    case MeetingState::Idle: return QStringLiteral("未接続");
    case MeetingState::Authenticating: return QStringLiteral("鍵を確認中");
    case MeetingState::Joining: return QStringLiteral("参加しています…");
    case MeetingState::WaitingForPermission: return QStringLiteral("参加中（許可待ち）");
    case MeetingState::Receiving: return QStringLiteral("参加中");
    case MeetingState::Failed: return QStringLiteral("失敗");
    case MeetingState::Ended: return QStringLiteral("会議が終わりました");
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
  return client_ != nullptr && !busy() && credentialsSaved() && parseMeetingNumber(app_.meetingId()) != 0 &&
         !app_.joinName().trimmed().isEmpty();
}

void MeetingController::join() {
  if (!canJoin()) return;
  backhaul::JoinRequest request;
  request.clientId = clientId_.toStdString();
  request.clientSecret = clientSecret_.toStdString();
  request.meetingNumber = parseMeetingNumber(app_.meetingId());
  request.displayName = app_.joinName().trimmed().toStdString();
  request.passcode = passcode_.toStdString();
  request.zak = zak_.trimmed().toStdString();
  request.audio = app_.handlesAudio();
  request.sendAudio = app_.sendsAudio() && !app_.micDeviceName().isEmpty();
  request.sendQuality = app_.sendQuality() == 2   ? backhaul::SendQuality::Music
                        : app_.sendQuality() == 1 ? backhaul::SendQuality::Original
                                                  : backhaul::SendQuality::Speech;
  request.sendStereo = app_.sendStereo();
  authenticated_ = false;
  detail_.clear();
  client_->join(request, this);  // reports through onMeetingState, also when it cannot start
}

bool MeetingController::sendAudio(const std::int16_t* samples, std::size_t frames, int channels) {
  // client_ is set once in the constructor and outlives every caller.
  return client_ != nullptr && client_->sendAudio(samples, frames, channels);
}

void MeetingController::leave() {
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
  detail_ = QString::fromStdString(detail);
  if (state == MeetingState::Joining) authenticated_ = true;
  const bool live = busy();
  if (live && !pollTimer_.isActive()) {
    pollTimer_.start();
    secondTimer_.start();
  }
  if (!live) {
    sendingAudio_ = false;
    sendNote_.clear();
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
      const QString text = name.isEmpty() ? QStringLiteral("共有中") : QStringLiteral("共有：%1").arg(name);
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
  for (const backhaul::Participant& p : everyone_) {
    ParticipantRow row;
    row.id = p.id;
    row.name = QString::fromStdString(p.name);
    row.isSelf = p.isSelf;
    row.role = p.isSelf ? QStringLiteral("自分") : p.isHost ? QStringLiteral("主催") : p.isCoHost ? QStringLiteral("共同") : QString();
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
    const QString note = QString::fromStdString(client_->sendAudioNote());
    if (sending != sendingAudio_ || note != sendNote_) {
      sendingAudio_ = sending;
      sendNote_ = note;
      emit changed();
    }
  }
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
