// Draws the newest picture of one slot, fitted into the item.
#pragma once

#include <QQuickItem>

class FrameStore;

class FrameItem : public QQuickItem {
  Q_OBJECT
  Q_PROPERTY(int slot READ slot WRITE setSlot NOTIFY slotChanged)
  // true: fill the item and cut what does not fit; false: show all with black bars.
  Q_PROPERTY(bool crop READ crop WRITE setCrop NOTIFY cropChanged)

 public:
  explicit FrameItem(QQuickItem* parent = nullptr);

  // Set once at start-up, before any item is created.
  static void setStore(FrameStore* store);

  int slot() const { return slot_; }
  void setSlot(int slot);
  bool crop() const { return crop_; }
  void setCrop(bool crop);

 signals:
  void slotChanged();
  void cropChanged();

 protected:
  QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData*) override;

 private:
  int slot_ = -1;
  bool crop_ = false;
};
