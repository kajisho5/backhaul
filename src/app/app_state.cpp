#include "app_state.h"
#include "dictionary_translator.h"

#include <QFontDatabase>
#include <QJsonArray>
#include <algorithm>
#include <cmath>

namespace {

// First family from `wanted` that this PC has; otherwise the system default.
QString pickFamily(const QStringList& wanted, QFontDatabase::SystemFont fallback) {
  const QStringList have = QFontDatabase::families();
  for (const QString& f : wanted) {
    if (have.contains(f)) return f;
  }
  return QFontDatabase::systemFont(fallback).family();
}

constexpr double kMinGainDb = -60.0;
constexpr double kMaxGainDb = 12.0;
constexpr int kMaxDelayMs = 2000;
constexpr int kMaxShareSlots = 4;
constexpr int kMaxChannels = 32;
constexpr int kMaxRememberedScreens = 64;

bool isShare(const SlotRow& r) { return r.situation.kind == backhaul::SlotKind::Share; }

}  // namespace

AppState::AppState(QObject* parent) : QObject(parent) {
  sans_ = pickFamily({QStringLiteral("Noto Sans JP"), QStringLiteral("Noto Sans CJK JP"), QStringLiteral("Yu Gothic UI"),
                      QStringLiteral("Meiryo UI")},
                     QFontDatabase::GeneralFont);
  mono_ = pickFamily({QStringLiteral("JetBrains Mono"), QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas"),
                      QStringLiteral("DejaVu Sans Mono")},
                     QFontDatabase::FixedFont);
  connect(&slots_, &QAbstractItemModel::dataChanged, this, [this] { refreshTwoShotSplit(); });
  connect(&slots_, &QAbstractItemModel::modelReset, this, [this] { refreshTwoShotSplit(); });
}

void AppState::setRole(const QString& key) {
  backhaul::Role role = role_;
  if (!backhaul::roleFromKey(key.toStdString(), &role) || role == role_) return;
  role_ = role;
  if (role_ == backhaul::Role::TwoShot && slots_.rows().size() - slots_.shareCount() < 2) addPersonSlot();
  if (role_ == backhaul::Role::Slide && shareRow() < 0) {  // a slide PC needs a slide slot
    SlotRow slide;
    slide.number = QStringLiteral("S");
    slide.label = T("スライド");
    slide.situation.kind = backhaul::SlotKind::Share;
    slide.cropTo169 = true;
    slots_.insertRow(static_cast<int>(slots_.rows().size()), slide);
    participants_.setSlotNumbers(slotNumbers());
  }
  if (!slotActive(selected_)) setSelected(0);
  // Slots the new role does not use hold nobody. Their Zoom names stay, so the people
  // come back by name when the role uses those slots again.
  for (int i = 0; i < slots_.rows().size(); ++i) {
    const SlotRow& row = slots_.rows()[i];
    if (isShare(row) || slotActive(i) || (!row.situation.assigned && row.userId == 0)) continue;
    participants_.clearSlot(i);
    slots_.update(i, [](SlotRow& r) {
      r.userId = 0;
      r.situation.assigned = r.situation.connected = false;
      r.situation.videoOn = r.situation.audioOn = r.situation.hasLastFrame = r.situation.unstable = false;
      r.width = r.height = r.fps = 0;
      r.level = 0.0;
    });
  }
  refreshDisplays();
  refreshTwoShotSplit();
  emit roleChanged();
  emit changed();
  emit settingsEdited();
}

bool AppState::slotActive(int row) const {
  if (row < 0 || row >= slots_.rows().size()) return false;
  if (isShare(slots_.rows()[row])) return backhaul::usesShare(role_) && row == shareRow();
  if (!backhaul::usesPeople(role_)) return false;
  const int people = backhaul::personSlotsOf(role_);
  // People come first in the list, so the role's slots are simply the first rows.
  return people == 0 || row < people;
}

int AppState::shareRow() const {
  for (int i = 0; i < slots_.rows().size(); ++i) {
    if (isShare(slots_.rows()[i])) return i;
  }
  return -1;
}

int AppState::roleRow() const { return role_ == backhaul::Role::Slide ? shareRow() : 0; }

void AppState::setTwoShotFill(bool fill) {
  if (fill == twoShotFill_) return;
  twoShotFill_ = fill;
  emit changed();
  emit settingsEdited();
}

void AppState::setTwoShotSolo(bool solo) {
  if (solo == twoShotSolo_) return;
  twoShotSolo_ = solo;
  refreshTwoShotSplit();
  emit changed();
  emit settingsEdited();
}

void AppState::swapTwoShot() {
  twoShotSwapped_ = !twoShotSwapped_;
  refreshTwoShotSplit();
  emit changed();
  emit settingsEdited();
}

void AppState::refreshTwoShotSplit() {
  auto hasPicture = [this](int row) {
    if (row < 0 || row >= slots_.rows().size()) return false;
    const QString out = slots_.data(slots_.index(row), SlotModel::VideoOutRole).toString();
    return out == QStringLiteral("live") || out == QStringLiteral("freeze");
  };
  int split = 0;
  if (role_ == backhaul::Role::TwoShot) {
    switch (backhaul::twoShotSplit(hasPicture(twoShotLeft()), hasPicture(twoShotRight()), twoShotSolo_)) {
      case backhaul::TwoShotSplit::Both: split = 0; break;
      case backhaul::TwoShotSplit::LeftOnly: split = 1; break;
      case backhaul::TwoShotSplit::RightOnly: split = 2; break;
    }
  }
  if (split == twoShotSplit_) return;
  twoShotSplit_ = split;
  emit changed();
}

QVector<int> AppState::pickRows() const {
  QVector<int> rows;
  const auto& people = participants_.rows();
  for (int i = 0; i < people.size(); ++i) {
    if (!people[i].isSelf) rows.push_back(i);
  }
  return rows;
}

QStringList AppState::pickNames() const {
  QStringList names{T("（だれも出さない）")};
  for (const int row : pickRows()) names.push_back(participants_.rows()[row].name);
  return names;
}

int AppState::pickOfSlot(int slotRow) const {
  const QVector<int> rows = pickRows();
  for (int i = 0; i < rows.size(); ++i) {
    if (participants_.rows()[rows[i]].slot == slotRow) return i + 1;
  }
  return 0;
}

void AppState::pickForSlot(int slotRow, int pick) {
  const QVector<int> rows = pickRows();
  if (pick < 0 || pick > rows.size() || !slotActive(slotRow) || isShare(slots_.rows()[slotRow])) return;
  if (pick > 0) {
    assignTo(slotRow, rows[pick - 1]);
    return;
  }
  participants_.clearSlot(slotRow);
  slots_.update(slotRow, [](SlotRow& r) {
    r.zoomName.clear();
    r.userId = 0;
    r.situation.assigned = r.situation.connected = false;
    r.situation.videoOn = r.situation.audioOn = r.situation.hasLastFrame = false;
    r.width = r.height = r.fps = 0;
    r.level = 0.0;
  });
  emit changed();
  emit settingsEdited();
  emit assignmentsChanged();
}

int AppState::roleOutputChoice() const {
  const QVector<int> rows = outputRows();
  for (int i = 0; i < rows.size(); ++i) {
    if (screens_[rows[i]].key == roleScreen_) return i + 1;
  }
  return 0;
}

void AppState::setRoleOutputChoice(int choice) {
  const QVector<int> rows = outputRows();
  if (choice < 0 || choice > rows.size()) return;
  const QString key = choice == 0 ? QString() : screens_[rows[choice - 1]].key;
  if (key == roleScreen_) return;
  roleScreen_ = key;
  refreshDisplays();
  emit changed();
  emit settingsEdited();
}

void AppState::setGridMode(bool on) {
  if (gridMode_ == on) return;
  gridMode_ = on;
  emit changed();
}

void AppState::setSelected(int row) {
  if (row < 0 || row >= slots_.rows().size() || row == selected_) return;
  selected_ = row;
  emit selectedChanged();
}

void AppState::edited(int row) {
  if (row >= 0 && row < slots_.rows().size()) emit settingsEdited();
}

void AppState::nudgeGain(int row, int steps) {
  slots_.update(row, [steps](SlotRow& r) {
    r.gainDb = std::clamp(std::round((r.gainDb + 0.5 * steps) * 2.0) / 2.0, kMinGainDb, kMaxGainDb);
  });
  edited(row);
}

void AppState::nudgeDelay(int row, int steps) {
  slots_.update(row, [steps](SlotRow& r) { r.delayMs = std::clamp(r.delayMs + 10 * steps, 0, kMaxDelayMs); });
  edited(row);
}

void AppState::setFreezeOnLoss(int row, bool freeze) {
  slots_.update(row, [freeze](SlotRow& r) {
    r.situation.onDisconnect = freeze ? backhaul::DisconnectPolicy::Freeze : backhaul::DisconnectPolicy::NameCard;
  });
  edited(row);
}

void AppState::assignToSelected(int participantRow) { assignTo(selected_, participantRow); }

void AppState::assignTo(int slot, int participantRow) {
  const auto& people = participants_.rows();
  if (participantRow < 0 || participantRow >= people.size() || !slotActive(slot) || isShare(slots_.rows()[slot])) return;
  if (people[participantRow].isSelf || people[participantRow].slot == slot) return;
  const QString name = people[participantRow].name;
  const quint32 id = people[participantRow].id;
  // The person leaves the slot they were in: one person is never in two slots.
  const int previous = people[participantRow].slot;
  if (previous >= 0 && previous != slot) {
    slots_.update(previous, [](SlotRow& r) {
      r.zoomName.clear();
      r.userId = 0;
      r.situation.assigned = r.situation.connected = false;
      r.situation.videoOn = r.situation.audioOn = r.situation.hasLastFrame = false;
      r.width = r.height = r.fps = 0;
      r.level = 0.0;
    });
  }
  participants_.clearSlot(slot);
  participants_.setSlotOf(participantRow, slot);
  slots_.update(slot, [&name, id](SlotRow& r) {
    r.zoomName = name;
    r.userId = id;
    r.situation.assigned = true;
    // Until the first picture and sound arrive, treat the person as present but silent and dark.
    r.situation.connected = true;
    r.situation.videoOn = false;
    r.situation.audioOn = false;
    r.situation.hasLastFrame = false;
    r.width = r.height = r.fps = 0;
    r.level = 0.0;
  });
  emit changed();
  emit settingsEdited();  // the Zoom name of the slot is kept for next time
  emit assignmentsChanged();
}

QString AppState::audioDevice() const {
  if (audioDeviceName_.isEmpty()) return QString();
  return QStringLiteral("%1（%2）").arg(audioDeviceName_, audioAsio_ ? QStringLiteral("ASIO") : QStringLiteral("WASAPI"));
}

void AppState::setAudioAsio(bool asio) {
  if (audioAsio_ == asio) return;
  audioAsio_ = asio;
  emit changed();
  emit settingsEdited();
}

bool AppState::cropSlides() const {
  for (const SlotRow& r : slots_.rows()) {
    if (isShare(r)) return r.cropTo169;
  }
  return true;
}

void AppState::setCropSlides(bool crop) {
  bool any = false;
  for (int i = 0; i < slots_.rows().size(); ++i) {
    if (!isShare(slots_.rows()[i]) || slots_.rows()[i].cropTo169 == crop) continue;
    slots_.update(i, [crop](SlotRow& r) { r.cropTo169 = crop; });
    any = true;
  }
  if (!any) return;
  emit changed();
  emit settingsEdited();
}

void AppState::setStandbyText(const QString& text) {
  const QString value = text.left(kMaxStandbyLength);
  if (value == standbyText_) return;
  standbyText_ = value;
  emit changed();
  emit settingsEdited();
}

void AppState::setPluginSettings(const QJsonObject& plugins) {
  if (plugins == pluginSettings_) return;
  pluginSettings_ = plugins;
  emit settingsEdited();
}

bool AppState::canAddSlot() const {
  return static_cast<int>(slots_.rows().size()) - slots_.shareCount() < kMaxPersonSlots;
}

void AppState::toggleRoute(int row, int channel) {
  if (channel < 1 || channel > audioChannels_ || channel > kMaxChannels) return;
  const quint32 bit = quint32{1} << (channel - 1);
  if (slots_.update(row, [bit](SlotRow& r) { r.channels ^= bit; })) emit settingsEdited();
}

void AppState::setLabel(int row, const QString& text) {
  const QString value = text.left(kMaxLabelLength);
  if (row < 0 || row >= slots_.rows().size() || slots_.rows()[row].label == value) return;
  slots_.update(row, [&value](SlotRow& r) { r.label = value; });
  refreshDisplays();
  emit changed();
  emit settingsEdited();
}

void AppState::setZoomName(int row, const QString& text) {
  const QString value = text.left(kMaxZoomNameLength);
  if (row < 0 || row >= slots_.rows().size() || slots_.rows()[row].zoomName == value) return;
  slots_.update(row, [&value](SlotRow& r) { r.zoomName = value; });
  emit settingsEdited();
}

QStringList AppState::slotNumbers() const {
  QStringList numbers;
  for (const SlotRow& r : slots_.rows()) numbers.push_back(r.number);
  return numbers;
}

void AppState::addPersonSlot() {
  if (!canAddSlot()) return;
  const auto& rows = slots_.rows();
  int at = 0;  // new people go right after the last person, before the slides
  for (int i = 0; i < rows.size(); ++i) {
    if (!isShare(rows[i])) at = i + 1;
  }
  const int number = static_cast<int>(rows.size()) - slots_.shareCount() + 1;
  SlotRow row;
  row.number = QString::number(number);
  row.label = T("演者%1").arg(number);
  row.situation.kind = backhaul::SlotKind::Person;
  slots_.insertRow(at, row);
  participants_.slotInserted(at, slotNumbers());
  for (auto it = slotByScreen_.begin(); it != slotByScreen_.end(); ++it) {
    if (it.value() >= at) ++it.value();
  }
  refreshDisplays();
  if (selected_ >= at) ++selected_;
  setSelected(at);
  emit selectedChanged();
  emit changed();
  emit settingsEdited();
}

QJsonObject AppState::settingsJson() const {
  QJsonArray slotArray;
  for (const SlotRow& r : slots_.rows()) {
    QJsonArray channels;
    for (int ch = 1; ch <= kMaxChannels; ++ch) {
      if ((r.channels & (quint32{1} << (ch - 1))) != 0) channels.push_back(ch);
    }
    QJsonObject slot;
    slot[QStringLiteral("kind")] = isShare(r) ? QStringLiteral("share") : QStringLiteral("person");
    slot[QStringLiteral("label")] = r.label;
    // A slide slot has no person set up for it; what it shows there is who shares right now.
    slot[QStringLiteral("zoomName")] = isShare(r) ? QString() : r.zoomName;
    slot[QStringLiteral("freezeOnLoss")] = r.situation.onDisconnect == backhaul::DisconnectPolicy::Freeze;
    slot[QStringLiteral("gainDb")] = r.gainDb;
    slot[QStringLiteral("delayMs")] = r.delayMs;
    slot[QStringLiteral("channels")] = channels;
    if (isShare(r)) slot[QStringLiteral("cropTo169")] = r.cropTo169;
    slotArray.push_back(slot);
  }
  QJsonObject audio;
  audio[QStringLiteral("method")] = audioAsio_ ? QStringLiteral("asio") : QStringLiteral("wasapi");
  audio[QStringLiteral("device")] = audioDeviceName_;
  audio[QStringLiteral("input")] = micDeviceName_;
  audio[QStringLiteral("sendQuality")] =
      sendQuality_ == 2 ? QStringLiteral("music") : sendQuality_ == 1 ? QStringLiteral("original") : QStringLiteral("speech");
  audio[QStringLiteral("sendStereo")] = sendStereo_;

  QJsonObject root;
  root[QStringLiteral("format")] = 1;
  root[QStringLiteral("audio")] = audio;
  root[QStringLiteral("slots")] = slotArray;
  QJsonArray outputs;
  for (auto it = slotByScreen_.constBegin(); it != slotByScreen_.constEnd(); ++it) {
    QJsonObject o;
    o[QStringLiteral("screen")] = it.key();
    o[QStringLiteral("slot")] = it.value();
    outputs.push_back(o);
  }
  root[QStringLiteral("outputs")] = outputs;
  root[QStringLiteral("gridScreen")] = gridScreen_;
  QJsonObject zoom;
  zoom[QStringLiteral("meetingId")] = meetingId_;
  zoom[QStringLiteral("name")] = joinName_;
  zoom[QStringLiteral("size")] = qualityAuto_ ? QStringLiteral("auto") : QStringLiteral("720");
  root[QStringLiteral("zoom")] = zoom;
  QJsonObject returning;
  returning[QStringLiteral("device")] = videoDeviceName_;
  returning[QStringLiteral("as")] = returnAsShare_ ? QStringLiteral("share") : QStringLiteral("camera");
  if (returnInAll_) returning[QStringLiteral("inAll")] = true;
  root[QStringLiteral("return")] = returning;
  root[QStringLiteral("standbyText")] = standbyText_;
  if (!pluginSettings_.isEmpty()) root[QStringLiteral("plugins")] = pluginSettings_;
  root[QStringLiteral("language")] = language_;
  if (!checksUpdates_) root[QStringLiteral("checksUpdates")] = false;
  root[QStringLiteral("role")] = role();
  root[QStringLiteral("roleScreen")] = roleScreen_;
  QJsonObject twoShot;
  twoShot[QStringLiteral("fill")] = twoShotFill_;
  twoShot[QStringLiteral("solo")] = twoShotSolo_;
  twoShot[QStringLiteral("swapped")] = twoShotSwapped_;
  root[QStringLiteral("twoShot")] = twoShot;
  return root;
}

bool AppState::applySettings(const QJsonObject& settings) {
  if (settings.value(QStringLiteral("format")).toInt() != 1) return false;
  const QJsonValue slotsValue = settings.value(QStringLiteral("slots"));
  const QJsonValue audioValue = settings.value(QStringLiteral("audio"));
  if (!slotsValue.isArray() || !audioValue.isObject()) return false;
  const QJsonObject audio = audioValue.toObject();
  const QString method = audio.value(QStringLiteral("method")).toString();
  if (method != QStringLiteral("asio") && method != QStringLiteral("wasapi")) return false;
  if (!audio.value(QStringLiteral("device")).isString()) return false;

  // Sending side: absent in files written before the sound role existed.
  QString micDevice;
  int sendQuality = 0;
  bool sendStereo = false;
  if (audio.contains(QStringLiteral("input"))) {
    const QString quality = audio.value(QStringLiteral("sendQuality")).toString();
    if (!audio.value(QStringLiteral("input")).isString() || !audio.value(QStringLiteral("sendStereo")).isBool() ||
        (quality != QStringLiteral("speech") && quality != QStringLiteral("original") && quality != QStringLiteral("music"))) {
      return false;
    }
    micDevice = audio.value(QStringLiteral("input")).toString().left(kMaxZoomNameLength);
    sendQuality = quality == QStringLiteral("music") ? 2 : quality == QStringLiteral("original") ? 1 : 0;
    sendStereo = audio.value(QStringLiteral("sendStereo")).toBool();
  }

  QVector<SlotRow> people;
  QVector<SlotRow> shares;
  const QJsonArray slotArray = slotsValue.toArray();
  for (const QJsonValue& value : slotArray) {
    if (!value.isObject()) return false;
    const QJsonObject slot = value.toObject();
    const QString kind = slot.value(QStringLiteral("kind")).toString();
    if (kind != QStringLiteral("person") && kind != QStringLiteral("share")) return false;
    const QJsonValue gain = slot.value(QStringLiteral("gainDb"));
    const QJsonValue delay = slot.value(QStringLiteral("delayMs"));
    const QJsonValue channels = slot.value(QStringLiteral("channels"));
    if (!slot.value(QStringLiteral("label")).isString() || !slot.value(QStringLiteral("zoomName")).isString() ||
        !slot.value(QStringLiteral("freezeOnLoss")).isBool() || !gain.isDouble() || !delay.isDouble() ||
        !channels.isArray()) {
      return false;
    }
    SlotRow r;
    r.situation.kind = kind == QStringLiteral("share") ? backhaul::SlotKind::Share : backhaul::SlotKind::Person;
    r.label = slot.value(QStringLiteral("label")).toString().left(kMaxLabelLength);
    r.zoomName = slot.value(QStringLiteral("zoomName")).toString().left(kMaxZoomNameLength);
    r.situation.onDisconnect = slot.value(QStringLiteral("freezeOnLoss")).toBool()
                                   ? backhaul::DisconnectPolicy::Freeze
                                   : backhaul::DisconnectPolicy::NameCard;
    const double gainDb = gain.toDouble();
    const double delayMs = delay.toDouble();
    if (!(gainDb >= kMinGainDb && gainDb <= kMaxGainDb) || !(delayMs >= 0.0 && delayMs <= kMaxDelayMs)) return false;
    r.gainDb = std::round(gainDb * 2.0) / 2.0;
    r.delayMs = static_cast<int>(std::lround(delayMs));
    const QJsonArray channelArray = channels.toArray();
    for (const QJsonValue& ch : channelArray) {
      const int n = ch.toInt(0);
      if (!ch.isDouble() || n < 1 || n > kMaxChannels) return false;
      r.channels |= quint32{1} << (n - 1);
    }
    if (isShare(r)) {
      const QJsonValue crop = slot.value(QStringLiteral("cropTo169"));
      if (!crop.isBool()) return false;
      r.cropTo169 = crop.toBool();
      r.zoomName.clear();  // older files may hold the name of whoever shared last
      shares.push_back(r);
    } else {
      people.push_back(r);
    }
  }
  if (people.isEmpty() || people.size() > kMaxPersonSlots || shares.size() > kMaxShareSlots) return false;

  // Monitor assignments: absent in files written before the video output page existed.
  QMap<QString, int> slotByScreen;
  const QJsonValue outputsValue = settings.value(QStringLiteral("outputs"));
  if (!outputsValue.isUndefined()) {
    if (!outputsValue.isArray()) return false;
    const QJsonArray outputs = outputsValue.toArray();
    if (outputs.size() > kMaxRememberedScreens) return false;
    for (const QJsonValue& value : outputs) {
      const QJsonObject o = value.toObject();
      const QJsonValue screen = o.value(QStringLiteral("screen"));
      const QJsonValue slot = o.value(QStringLiteral("slot"));
      const int row = slot.toInt(-1);
      if (!screen.isString() || screen.toString().isEmpty() || !slot.isDouble() || row < 0 ||
          row >= people.size() + shares.size()) {
        return false;
      }
      slotByScreen.insert(screen.toString().left(kMaxZoomNameLength), row);
    }
  }
  const QJsonValue gridValue = settings.value(QStringLiteral("gridScreen"));
  if (!gridValue.isUndefined() && !gridValue.isString()) return false;
  // Zoom page: absent in older files.
  QString meetingId = meetingId_, joinName = joinName_;
  bool qualityAuto = qualityAuto_;
  const QJsonValue zoomValue = settings.value(QStringLiteral("zoom"));
  if (!zoomValue.isUndefined()) {
    const QJsonObject zoom = zoomValue.toObject();
    const QString size = zoom.value(QStringLiteral("size")).toString();
    if (!zoomValue.isObject() || !zoom.value(QStringLiteral("meetingId")).isString() ||
        !zoom.value(QStringLiteral("name")).isString() ||
        (size != QStringLiteral("auto") && size != QStringLiteral("720"))) {
      return false;
    }
    meetingId = zoom.value(QStringLiteral("meetingId")).toString().left(kMaxLabelLength);
    joinName = zoom.value(QStringLiteral("name")).toString().left(kMaxLabelLength);
    qualityAuto = size == QStringLiteral("auto");
  }

  // Role: absent in files written before roles existed (those PCs did everything).
  backhaul::Role role = backhaul::Role::All;
  QString roleScreen;
  bool fill = true, solo = false, swapped = false;
  const QJsonValue roleValue = settings.value(QStringLiteral("role"));
  if (!roleValue.isUndefined()) {
    const QJsonValue screen = settings.value(QStringLiteral("roleScreen"));
    const QJsonValue twoShotValue = settings.value(QStringLiteral("twoShot"));
    const QJsonObject twoShot = twoShotValue.toObject();
    if (!roleValue.isString() || !backhaul::roleFromKey(roleValue.toString().toStdString(), &role) ||
        !screen.isString() || !twoShotValue.isObject() || !twoShot.value(QStringLiteral("fill")).isBool() ||
        !twoShot.value(QStringLiteral("solo")).isBool() || !twoShot.value(QStringLiteral("swapped")).isBool()) {
      return false;
    }
    roleScreen = screen.toString().left(kMaxZoomNameLength);
    fill = twoShot.value(QStringLiteral("fill")).toBool();
    solo = twoShot.value(QStringLiteral("solo")).toBool();
    swapped = twoShot.value(QStringLiteral("swapped")).toBool();
  }
  if (role == backhaul::Role::TwoShot && people.size() < 2) return false;

  // Standby text of the slide output: absent in older files, which then get the usual word.
  QString standbyText = QStringLiteral("SLIDE");
  const QJsonValue standbyValue = settings.value(QStringLiteral("standbyText"));
  if (!standbyValue.isUndefined()) {
    if (!standbyValue.isString()) return false;
    standbyText = standbyValue.toString().left(kMaxStandbyLength);
  }

  // Return picture: absent in files written before that role existed.
  QString videoDevice;
  // Language: absent in files written before there was a choice; those PCs keep what they show now.
  QString language = language_;
  const QJsonValue languageValue = settings.value(QStringLiteral("language"));
  if (!languageValue.isUndefined()) {
    language = languageValue.toString();
    if (language != QStringLiteral("en") && language != QStringLiteral("ja")) return false;
  }
  // Looking for updates: absent in files written before there was an updater, and then on.
  bool checksUpdates = true;
  const QJsonValue checksValue = settings.value(QStringLiteral("checksUpdates"));
  if (!checksValue.isUndefined()) {
    if (!checksValue.isBool()) return false;
    checksUpdates = checksValue.toBool();
  }
  bool returnAsShare = false;
  bool returnInAll = false;
  const QJsonValue returnValue = settings.value(QStringLiteral("return"));
  if (!returnValue.isUndefined()) {
    const QJsonObject returning = returnValue.toObject();
    const QString as = returning.value(QStringLiteral("as")).toString();
    if (!returnValue.isObject() || !returning.value(QStringLiteral("device")).isString() ||
        (as != QStringLiteral("camera") && as != QStringLiteral("share"))) {
      return false;
    }
    videoDevice = returning.value(QStringLiteral("device")).toString().left(kMaxZoomNameLength);
    returnAsShare = as == QStringLiteral("share");
    // Absent in files written before the all-in-one PC could send the return.
    const QJsonValue inAll = returning.value(QStringLiteral("inAll"));
    if (!inAll.isUndefined() && !inAll.isBool()) return false;
    returnInAll = inAll.toBool();
  }

  // People first, then slides, numbered in order.
  QVector<SlotRow> rows;
  for (int i = 0; i < people.size(); ++i) {
    people[i].number = QString::number(i + 1);
    rows.push_back(people[i]);
  }
  for (int i = 0; i < shares.size(); ++i) {
    shares[i].number = i == 0 ? QStringLiteral("S") : QStringLiteral("S%1").arg(i + 1);
    rows.push_back(shares[i]);
  }
  slots_.setRows(rows);
  participants_.setRows({}, slotNumbers());
  // Plug-ins: absent in older files. What is inside is checked by the plug-in part itself.
  const QJsonValue pluginsValue = settings.value(QStringLiteral("plugins"));
  if (!pluginsValue.isUndefined() && !pluginsValue.isObject()) return false;

  slotByScreen_ = slotByScreen;
  pluginSettings_ = pluginsValue.toObject();
  gridScreen_ = gridValue.toString().left(kMaxZoomNameLength);
  meetingId_ = meetingId;
  joinName_ = joinName;
  qualityAuto_ = qualityAuto;
  standbyText_ = standbyText;
  videoDeviceName_ = videoDevice;
  returnAsShare_ = returnAsShare;
  returnInAll_ = returnInAll;
  checksUpdates_ = checksUpdates;
  if (language != language_) {
    language_ = language;
    emit languageChanged();
  }
  role_ = role;
  roleScreen_ = roleScreen;
  twoShotFill_ = fill;
  twoShotSolo_ = solo;
  twoShotSwapped_ = swapped;
  refreshDisplays();
  refreshTwoShotSplit();
  emit roleChanged();
  audioAsio_ = method == QStringLiteral("asio");
  audioDeviceName_ = audio.value(QStringLiteral("device")).toString().left(kMaxZoomNameLength);
  micDeviceName_ = micDevice;
  sendQuality_ = sendQuality;
  sendStereo_ = sendStereo;
  selected_ = 0;
  emit selectedChanged();
  emit changed();
  return true;
}

void AppState::loadDefaults() {
  QVector<SlotRow> rows;
  for (int i = 1; i <= 4; ++i) {
    SlotRow r;
    r.number = QString::number(i);
    r.label = T("演者%1").arg(i);
    r.situation.kind = backhaul::SlotKind::Person;
    r.channels = quint32{1} << (i - 1);
    rows.push_back(r);
  }
  SlotRow slide;
  slide.number = QStringLiteral("S");
  slide.label = T("スライド");
  slide.situation.kind = backhaul::SlotKind::Share;
  slide.cropTo169 = true;
  slide.channels = quint32{1} << 4;
  rows.push_back(slide);
  slots_.setRows(rows);
  participants_.setRows({}, slotNumbers());
  slotByScreen_.clear();
  gridScreen_.clear();
  standbyText_ = QStringLiteral("SLIDE");
  pluginSettings_ = QJsonObject();
  videoDeviceName_.clear();
  returnAsShare_ = false;
  returnInAll_ = false;
  role_ = backhaul::Role::Unset;
  roleScreen_.clear();
  twoShotFill_ = true;
  twoShotSolo_ = twoShotSwapped_ = false;
  refreshDisplays();
  refreshTwoShotSplit();
  emit roleChanged();
  audioAsio_ = false;
  audioDeviceName_.clear();
  micDeviceName_.clear();
  sendQuality_ = 0;
  sendStereo_ = false;
  selected_ = 0;
  emit selectedChanged();
  emit changed();
}

void AppState::setScreens(const QVector<ScreenInfo>& screens, const QString& operatorKey) {
  screens_ = screens;
  operatorKey_ = operatorKey;
  refreshDisplays();
  emit changed();
}

QVector<int> AppState::outputRows() const {
  QVector<int> rows;
  for (int i = 0; i < screens_.size(); ++i) {
    if (screens_[i].key != operatorKey_) rows.push_back(i);
  }
  return rows;
}

void AppState::refreshDisplays() {
  const auto& slotRows = slots_.rows();
  QVector<DisplayRow> rows;
  QVector<QStringList> shownOn(slotRows.size());
  for (const ScreenInfo& s : screens_) {
    DisplayRow r;
    r.key = s.key;
    r.title = s.title;
    r.mode = s.mode;
    r.isOperator = s.key == operatorKey_;
    // Single-output roles send their one picture to one monitor; the slot-by-monitor
    // table belongs to the "everything" role and is left alone meanwhile.
    const bool single = backhaul::singleOutput(role_);
    // A PC whose role shows no pictures (sound, return) opens no output window at all,
    // whatever monitors were set up for another role.
    const bool pictures = backhaul::receivesVideo(role_) || role_ == backhaul::Role::Unset;
    const int slot = !pictures ? -1
                     : single  ? (s.key == roleScreen_ && roleRow() >= 0 && roleRow() < slotRows.size() ? roleRow() : -1)
                               : slotByScreen_.value(s.key, -1);
    if (r.isOperator) {
      r.slotText = T("操作画面（このPCの画面）");
    } else if (slot >= 0 && slot < slotRows.size()) {
      r.slot = slot;
      const bool share = slotRows[slot].situation.kind == backhaul::SlotKind::Share;
      r.slotText = QStringLiteral("%1%2　%3").arg(share ? QString() : T("枠"), slotRows[slot].number, slotRows[slot].label);
      shownOn[slot].push_back(s.title);
    } else {
      r.slotText = T("出さない（黒）");
    }
    rows.push_back(r);
  }
  displays_.setRows(rows);
  for (int i = 0; i < slotRows.size(); ++i) {
    const QString text = shownOn[i].join(QStringLiteral("・"));
    if (slotRows[i].display != text) slots_.update(i, [&text](SlotRow& r) { r.display = text; });
  }
}

QStringList AppState::outputChoices() const {
  QStringList choices{T("出さない（黒）")};
  for (const SlotRow& r : slots_.rows()) {
    const bool share = r.situation.kind == backhaul::SlotKind::Share;
    choices.push_back(QStringLiteral("%1%2　%3").arg(share ? QString() : T("枠"), r.number, r.label));
  }
  return choices;
}

void AppState::setDisplayChoice(int displayRow, int choice) {
  if (displayRow < 0 || displayRow >= screens_.size() || choice < 0 || choice > slots_.rows().size()) return;
  const QString key = screens_[displayRow].key;
  if (key == operatorKey_) return;
  const int slot = choice - 1;
  if (slotByScreen_.value(key, -1) == slot) return;
  if (slot < 0) {
    slotByScreen_.remove(key);
  } else {
    slotByScreen_.insert(key, slot);
  }
  refreshDisplays();
  emit changed();
  emit settingsEdited();
}

QStringList AppState::gridChoices() const {
  QStringList choices{T("出さない")};
  for (const int row : outputRows()) choices.push_back(screens_[row].title);
  return choices;
}

int AppState::gridChoice() const {
  const QVector<int> rows = outputRows();
  for (int i = 0; i < rows.size(); ++i) {
    if (screens_[rows[i]].key == gridScreen_) return i + 1;
  }
  return 0;
}

void AppState::setGridChoice(int choice) {
  const QVector<int> rows = outputRows();
  if (choice < 0 || choice > rows.size()) return;
  const QString key = choice == 0 ? QString() : screens_[rows[choice - 1]].key;
  if (key == gridScreen_) return;
  gridScreen_ = key;
  emit changed();
  emit settingsEdited();
}

void AppState::setMeetingId(const QString& text) {
  const QString value = text.left(kMaxLabelLength);
  if (value == meetingId_) return;
  meetingId_ = value;
  emit changed();
  emit settingsEdited();
}

void AppState::setJoinName(const QString& text) {
  const QString value = text.left(kMaxLabelLength);
  if (value == joinName_) return;
  joinName_ = value;
  emit changed();
  emit settingsEdited();
}

void AppState::setQualityAuto(bool on) {
  if (on == qualityAuto_) return;
  qualityAuto_ = on;
  emit changed();
  emit settingsEdited();
}

void AppState::setVideoDeviceName(const QString& name) {
  const QString value = name.left(kMaxZoomNameLength);
  if (value == videoDeviceName_) return;
  videoDeviceName_ = value;
  emit changed();
  emit settingsEdited();
}

void AppState::setReturnAsShare(bool share) {
  if (share == returnAsShare_) return;
  returnAsShare_ = share;
  emit changed();
  emit settingsEdited();
}

void AppState::setLanguage(const QString& language) {
  if (language == language_ || (language != QStringLiteral("en") && language != QStringLiteral("ja"))) return;
  language_ = language;
  emit languageChanged();
  emit changed();
  emit settingsEdited();
}

void AppState::setChecksUpdates(bool on) {
  if (on == checksUpdates_) return;
  checksUpdates_ = on;
  emit changed();
  emit settingsEdited();
}

void AppState::setReturnInAll(bool on) {
  if (on == returnInAll_) return;
  returnInAll_ = on;
  emit roleChanged();  // what this PC sends has changed, as it does with a change of role
  emit changed();
  emit settingsEdited();
}

void AppState::setMicDeviceName(const QString& name) {
  const QString value = name.left(kMaxZoomNameLength);
  if (value == micDeviceName_) return;
  micDeviceName_ = value;
  emit changed();
  emit settingsEdited();
}

void AppState::setSendQuality(int quality) {
  if (quality < 0 || quality > 2 || quality == sendQuality_) return;
  sendQuality_ = quality;
  emit changed();
  emit settingsEdited();
}

void AppState::setSendStereo(bool stereo) {
  if (stereo == sendStereo_) return;
  sendStereo_ = stereo;
  emit changed();
  emit settingsEdited();
}

void AppState::setAudioDeviceName(const QString& name) {
  const QString value = name.left(kMaxZoomNameLength);
  if (value == audioDeviceName_) return;
  audioDeviceName_ = value;
  emit changed();
  emit settingsEdited();
}
