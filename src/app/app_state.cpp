#include "app_state.h"

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

void AppState::assignToSelected(int participantRow) {
  const auto& people = participants_.rows();
  if (participantRow < 0 || participantRow >= people.size()) return;
  const int slot = selected_;
  const QString name = people[participantRow].name;
  participants_.clearSlot(slot);
  participants_.setSlotOf(participantRow, slot);
  slots_.update(slot, [&name](SlotRow& r) {
    r.zoomName = name;
    r.situation.assigned = true;
    // Until the first picture and sound arrive, treat the person as present but silent and dark.
    r.situation.connected = true;
    r.situation.videoOn = false;
    r.situation.audioOn = false;
    r.situation.hasLastFrame = false;
    r.width = r.height = r.fps = 0;
    r.level = 0.0;
  });
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
  row.label = QStringLiteral("演者%1").arg(number);
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
    slot[QStringLiteral("zoomName")] = r.zoomName;
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
  slotByScreen_ = slotByScreen;
  gridScreen_ = gridValue.toString().left(kMaxZoomNameLength);
  refreshDisplays();
  audioAsio_ = method == QStringLiteral("asio");
  audioDeviceName_ = audio.value(QStringLiteral("device")).toString().left(kMaxZoomNameLength);
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
    r.label = QStringLiteral("演者%1").arg(i);
    r.situation.kind = backhaul::SlotKind::Person;
    r.channels = quint32{1} << (i - 1);
    rows.push_back(r);
  }
  SlotRow slide;
  slide.number = QStringLiteral("S");
  slide.label = QStringLiteral("スライド");
  slide.situation.kind = backhaul::SlotKind::Share;
  slide.cropTo169 = true;
  slide.channels = quint32{1} << 4;
  rows.push_back(slide);
  slots_.setRows(rows);
  participants_.setRows({}, slotNumbers());
  slotByScreen_.clear();
  gridScreen_.clear();
  refreshDisplays();
  audioAsio_ = true;
  audioDeviceName_.clear();
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
    const int slot = slotByScreen_.value(s.key, -1);
    if (r.isOperator) {
      r.slotText = QStringLiteral("操作画面（このPCの画面）");
    } else if (slot >= 0 && slot < slotRows.size()) {
      r.slot = slot;
      const bool share = slotRows[slot].situation.kind == backhaul::SlotKind::Share;
      r.slotText = QStringLiteral("%1%2　%3").arg(share ? QString() : QStringLiteral("枠"), slotRows[slot].number, slotRows[slot].label);
      shownOn[slot].push_back(s.title);
    } else {
      r.slotText = QStringLiteral("出さない（黒）");
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
  QStringList choices{QStringLiteral("出さない（黒）")};
  for (const SlotRow& r : slots_.rows()) {
    const bool share = r.situation.kind == backhaul::SlotKind::Share;
    choices.push_back(QStringLiteral("%1%2　%3").arg(share ? QString() : QStringLiteral("枠"), r.number, r.label));
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
  QStringList choices{QStringLiteral("出さない")};
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
