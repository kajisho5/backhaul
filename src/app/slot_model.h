// The list of slots as the operator screen sees it.
#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

#include "core/slot_output.h"

struct SlotRow {
  QString number;        // "1", "2", ... or "S" for the slide
  QString label;         // on-air name
  QString zoomName;      // who is assigned (empty = nobody)
  backhaul::SlotSituation situation;
  int width = 0;         // picture size currently arriving (0 = none)
  int height = 0;
  int fps = 0;
  bool cropTo169 = false;  // slide only: cut to 16:9
  double level = 0.0;      // meter, 0..1
  double gainDb = 0.0;
  int delayMs = 0;
  QString display;       // e.g. "HDMI 2"
  quint32 channels = 0;  // audio output channels: bit 0 = ch 1, bit 1 = ch 2, ...
};

class SlotModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum Role {
    NumberRole = Qt::UserRole + 1,
    LabelRole,
    ZoomNameRole,
    StateRole,        // "unassigned" | "ok" | "camoff" | "mute" | "unstable" | "lost"
    StateTextRole,    // the same in words
    AlsoMutedRole,
    VideoOutRole,     // "black" | "live" | "card" | "freeze"
    IsShareRole,
    SizeTextRole,     // "1920×1080", "カメラオフ", ...
    DetailTextRole,   // "30コマ", "16:9に切り取り", ...
    LevelRole,
    GainRole,
    DelayRole,
    DisplayRole,
    ChannelTextRole,  // "ch 1", "ch 1・3", or empty when the slot goes nowhere
    ChannelsRole,     // the same as a bit mask (bit 0 = ch 1)
    FreezeOnLossRole,
    GridRowRole,     // where the tile sits: people fill the two left columns,
    GridColumnRole,  // slides stack in the right column
  };

  explicit SlotModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setRows(QVector<SlotRow> rows);
  const QVector<SlotRow>& rows() const { return rows_; }
  // Applies `change` to one row and tells the views. Returns false for a bad index.
  template <typename F>
  bool update(int row, F change) {
    if (row < 0 || row >= rows_.size()) return false;
    change(rows_[row]);
    const QModelIndex i = index(row);
    emit dataChanged(i, i);
    return true;
  }

  void insertRow(int at, SlotRow row);

  // Row in the right column just below the last slide (where the return tile goes).
  int shareCount() const;

  static QString stateKey(backhaul::UiState s);
  static QString stateText(backhaul::UiState s);
  static QString channelText(quint32 channels);

 private:
  QVector<SlotRow> rows_;
};
