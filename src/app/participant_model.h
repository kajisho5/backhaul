// Everyone in the meeting, for assigning people to slots.
#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

struct ParticipantRow {
  QString name;
  QString role;   // "主催" / "共同" / "自分" / ""
  int slot = -1;  // index of the slot this person is in, -1 = none
  bool isSelf = false;
};

class ParticipantModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum Role { NameRole = Qt::UserRole + 1, TagRole, CanAssignRole };

  explicit ParticipantModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setRows(QVector<ParticipantRow> rows, QStringList slotNumbers);
  const QVector<ParticipantRow>& rows() const { return rows_; }
  void setSlotOf(int row, int slot);
  void clearSlot(int slot);
  // A slot was inserted at index `at`: people in later slots keep their slot.
  void slotInserted(int at, QStringList slotNumbers);
  void setSlotNumbers(QStringList slotNumbers);

 private:
  QVector<ParticipantRow> rows_;
  QStringList slotNumbers_;
};
