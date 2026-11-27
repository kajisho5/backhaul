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
  beginResetModel();
  rows_ = std::move(rows);
  endResetModel();
}
