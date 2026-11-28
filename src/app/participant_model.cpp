#include "participant_model.h"
#include "dictionary_translator.h"

ParticipantModel::ParticipantModel(QObject* parent) : QAbstractListModel(parent) {}

int ParticipantModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant ParticipantModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) return {};
  const ParticipantRow& r = rows_[index.row()];
  switch (role) {
    case NameRole: return r.name;
    case TagRole:
      if (r.slot >= 0 && r.slot < slotNumbers_.size()) return T("枠%1").arg(slotNumbers_[r.slot]);
      return r.role;
    case CanAssignRole: return r.slot < 0 && r.role.isEmpty() && !r.isSelf;
    case InSlotRole: return r.slot >= 0;
    case IsSelfRole: return r.isSelf;
  }
  return {};
}

QHash<int, QByteArray> ParticipantModel::roleNames() const {
  return {{NameRole, "name"}, {TagRole, "tag"}, {CanAssignRole, "canAssign"}, {InSlotRole, "inSlot"}, {IsSelfRole, "isSelf"}};
}

void ParticipantModel::setRows(QVector<ParticipantRow> rows, QStringList slotNumbers) {
  beginResetModel();
  rows_ = std::move(rows);
  slotNumbers_ = std::move(slotNumbers);
  endResetModel();
}

void ParticipantModel::setSlotOf(int row, int slot) {
  if (row < 0 || row >= rows_.size()) return;
  rows_[row].slot = slot;
  emit dataChanged(index(row), index(row));
}

void ParticipantModel::clearSlot(int slot) {
  for (int i = 0; i < rows_.size(); ++i) {
    if (rows_[i].slot == slot) setSlotOf(i, -1);
  }
}

void ParticipantModel::slotInserted(int at, QStringList slotNumbers) {
  for (ParticipantRow& r : rows_) {
    if (r.slot >= at) ++r.slot;
  }
  setSlotNumbers(std::move(slotNumbers));
}

void ParticipantModel::setSlotNumbers(QStringList slotNumbers) {
  slotNumbers_ = std::move(slotNumbers);
  if (!rows_.isEmpty()) emit dataChanged(index(0), index(static_cast<int>(rows_.size()) - 1));
}
