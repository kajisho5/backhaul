#include "app_state.h"

#include <QFontDatabase>
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

void AppState::nudgeGain(int row, int steps) {
  slots_.update(row, [steps](SlotRow& r) {
    r.gainDb = std::clamp(std::round((r.gainDb + 0.5 * steps) * 2.0) / 2.0, -60.0, 12.0);
  });
}

void AppState::nudgeDelay(int row, int steps) {
  slots_.update(row, [steps](SlotRow& r) { r.delayMs = std::clamp(r.delayMs + 10 * steps, 0, 2000); });
}

void AppState::setFreezeOnLoss(int row, bool freeze) {
  slots_.update(row, [freeze](SlotRow& r) {
    r.situation.onDisconnect = freeze ? backhaul::DisconnectPolicy::Freeze : backhaul::DisconnectPolicy::NameCard;
  });
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
