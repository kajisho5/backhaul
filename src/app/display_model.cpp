#include "display_model.h"

DisplayModel::DisplayModel(QObject* parent) : QAbstractListModel(parent) {}

int DisplayModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant DisplayModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) return {};
  const DisplayRow& r = rows_[index.row()];
  switch (role) {
    case NumberRole: return index.row() + 1;
    case TitleRole: return r.title;
    case ModeRole: return r.mode;
    case IsOperatorRole: return r.isOperator;
    case SlotRole: return r.slot;
    case SlotTextRole: return r.slotText;
  }
  return {};
}

QHash<int, QByteArray> DisplayModel::roleNames() const {
  return {{NumberRole, "number"},         {TitleRole, "title"}, {ModeRole, "mode"},
          {IsOperatorRole, "isOperator"}, {SlotRole, "slot"},   {SlotTextRole, "slotText"}};
}

void DisplayModel::setRows(QVector<DisplayRow> rows) {
  // Same monitors as before: update in place, so the output windows that follow this
  // list are not torn down and rebuilt (that would flash on air).
  bool sameMonitors = rows.size() == rows_.size();
  for (int i = 0; sameMonitors && i < rows.size(); ++i) sameMonitors = rows[i].key == rows_[i].key;
  if (sameMonitors) {
    rows_ = std::move(rows);
    if (!rows_.isEmpty()) emit dataChanged(index(0), index(static_cast<int>(rows_.size()) - 1));
    return;
  }
  beginResetModel();
  rows_ = std::move(rows);
  endResetModel();
}
