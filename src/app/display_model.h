// The monitors connected to this PC, and which slot each one shows full screen.
#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

struct DisplayRow {
  QString key;       // what the assignment is remembered by (the system's name for the output)
  QString title;     // what the operator reads
  QString mode;      // "1920×1080・60Hz"
  bool isOperator = false;  // the monitor the operator screen is on: never an output
  int slot = -1;     // slot row shown here, -1 = black
  QString slotText;  // "枠1　架空 一郎", or the text for black / operator screen
};

class DisplayModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum Role { NumberRole = Qt::UserRole + 1, TitleRole, ModeRole, IsOperatorRole, SlotRole, SlotTextRole };

  explicit DisplayModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setRows(QVector<DisplayRow> rows);
  const QVector<DisplayRow>& rows() const { return rows_; }

 private:
  QVector<DisplayRow> rows_;
};
