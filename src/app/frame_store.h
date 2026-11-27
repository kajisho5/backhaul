// The newest picture of each slot, shared between the thread that receives video
// and the windows that show it.
#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>

class FrameStore : public QObject {
  Q_OBJECT
 public:
  explicit FrameStore(QObject* parent = nullptr);

  // Any thread.
  void put(int slot, const QImage& image);
  QImage latest(int slot) const;
  void clear(int slot);
  void clearAll();
  // Pictures received for the slot since the last call (for the frame-rate display).
  int takeCount(int slot);

 signals:
  void frameReady(int slot);

 private:
  struct Entry {
    QImage image;
    int count = 0;
  };
  mutable QMutex mutex_;
  QHash<int, Entry> entries_;
};
