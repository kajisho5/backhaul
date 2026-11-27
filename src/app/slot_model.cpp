#include "slot_model.h"

using backhaul::SlotDecision;
using backhaul::UiState;
using backhaul::VideoOut;

SlotModel::SlotModel(QObject* parent) : QAbstractListModel(parent) {}

int SlotModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QString SlotModel::stateKey(UiState s) {
  switch (s) {
    case UiState::Unassigned: return QStringLiteral("unassigned");
    case UiState::Ok: return QStringLiteral("ok");
    case UiState::CameraOff: return QStringLiteral("camoff");
    case UiState::Muted: return QStringLiteral("mute");
    case UiState::Unstable: return QStringLiteral("unstable");
    case UiState::Lost: return QStringLiteral("lost");
  }
  return QString();
}

QString SlotModel::stateText(UiState s) {
  switch (s) {
    case UiState::Unassigned: return QStringLiteral("未割り当て");
    case UiState::Ok: return QStringLiteral("正常");
    case UiState::CameraOff: return QStringLiteral("カメラオフ");
    case UiState::Muted: return QStringLiteral("ミュート");
    case UiState::Unstable: return QStringLiteral("回線不安定");
    case UiState::Lost: return QStringLiteral("切断");
  }
  return QString();
}

QVariant SlotModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) return {};
  const SlotRow& r = rows_[index.row()];
  const SlotDecision d = backhaul::decide(r.situation);
  const bool share = r.situation.kind == backhaul::SlotKind::Share;

  switch (role) {
    case NumberRole: return r.number;
    case LabelRole: return r.label;
    case ZoomNameRole: return r.zoomName;
    case StateRole: return stateKey(d.ui);
    case StateTextRole: return stateText(d.ui);
    case AlsoMutedRole: return d.uiMuted;
    case VideoOutRole:
      switch (d.video) {
        case VideoOut::Black: return QStringLiteral("black");
        case VideoOut::Live: return QStringLiteral("live");
        case VideoOut::NameCard: return QStringLiteral("card");
        case VideoOut::Freeze: return QStringLiteral("freeze");
      }
      return {};
    case IsShareRole: return share;
    case SizeTextRole:
      if (d.video == VideoOut::Live && r.width > 0) return QStringLiteral("%1×%2").arg(r.width).arg(r.height);
      if (d.video == VideoOut::NameCard) return QStringLiteral("名前カード");
      if (d.video == VideoOut::Freeze) return QStringLiteral("静止");
      return share ? QStringLiteral("共有なし") : QStringLiteral("映像なし");
    case DetailTextRole:
      if (d.video != VideoOut::Live) return QString();
      if (share) return r.cropTo169 ? QStringLiteral("16:9に切り取り") : QStringLiteral("全体を表示");
      return r.fps > 0 ? QStringLiteral("%1コマ").arg(r.fps) : QString();
    case LevelRole: return d.audio ? r.level : 0.0;
    case GainRole: return r.gainDb;
    case DelayRole: return r.delayMs;
    case DisplayRole: return r.display;
    case ChannelTextRole: return channelText(r.channels);
    case ChannelsRole: return static_cast<int>(r.channels);
    case CropRole: return share && r.cropTo169;
    case FreezeOnLossRole: return r.situation.onDisconnect == backhaul::DisconnectPolicy::Freeze;
    case GridRowRole:
    case GridColumnRole: {
      int before = 0;  // tiles of the same kind earlier in the list
      for (int i = 0; i < index.row(); ++i) {
        if ((rows_[i].situation.kind == backhaul::SlotKind::Share) == share) ++before;
      }
      if (role == GridRowRole) return share ? before : before / 2;
      return share ? 2 : before % 2;
    }
  }
  return {};
}

QHash<int, QByteArray> SlotModel::roleNames() const {
  return {
      {NumberRole, "number"},       {LabelRole, "label"},         {ZoomNameRole, "zoomName"},
      {StateRole, "state"},         {StateTextRole, "stateText"}, {AlsoMutedRole, "alsoMuted"},
      {VideoOutRole, "videoOut"},   {IsShareRole, "isShare"},     {SizeTextRole, "sizeText"},
      {DetailTextRole, "detailText"}, {LevelRole, "level"},       {GainRole, "gainDb"},
      {DelayRole, "delayMs"},       {DisplayRole, "display"},     {ChannelTextRole, "channelText"},
      {ChannelsRole, "channels"},
      {FreezeOnLossRole, "freezeOnLoss"}, {GridRowRole, "gridRow"}, {GridColumnRole, "gridColumn"},
      {CropRole, "crop"},
  };
}

void SlotModel::setRows(QVector<SlotRow> rows) {
  beginResetModel();
  rows_ = std::move(rows);
  endResetModel();
}

void SlotModel::insertRow(int at, SlotRow row) {
  if (at < 0 || at > rows_.size()) return;
  beginInsertRows(QModelIndex(), at, at);
  rows_.insert(at, std::move(row));
  endInsertRows();
  // Tiles after the new one move, so their grid position changes too.
  if (at + 1 < rows_.size()) emit dataChanged(index(at + 1), index(static_cast<int>(rows_.size()) - 1));
}

QString SlotModel::channelText(quint32 channels) {
  QString text;
  for (int ch = 1; ch <= 32; ++ch) {
    if ((channels & (quint32{1} << (ch - 1))) == 0) continue;
    text += text.isEmpty() ? QStringLiteral("ch %1").arg(ch) : QStringLiteral("・%1").arg(ch);
  }
  return text;
}

int SlotModel::shareCount() const {
  int n = 0;
  for (const SlotRow& r : rows_) {
    if (r.situation.kind == backhaul::SlotKind::Share) ++n;
  }
  return n;
}
