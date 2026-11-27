#include "frame_store.h"

#include <QMutexLocker>

FrameStore::FrameStore(QObject* parent) : QObject(parent) {}

void FrameStore::put(int slot, const QImage& image) {
  {
    const QMutexLocker lock(&mutex_);
    Entry& e = entries_[slot];
    e.image = image;
    ++e.count;
  }
  emit frameReady(slot);
}

QImage FrameStore::latest(int slot) const {
  const QMutexLocker lock(&mutex_);
  return entries_.value(slot).image;
}

void FrameStore::clear(int slot) {
  {
    const QMutexLocker lock(&mutex_);
    entries_.remove(slot);
  }
  emit frameReady(slot);
}

void FrameStore::clearAll() {
  QList<int> cleared;
  {
    const QMutexLocker lock(&mutex_);
    cleared = entries_.keys();
    entries_.clear();
  }
  for (const int slot : cleared) emit frameReady(slot);
}

int FrameStore::takeCount(int slot) {
  const QMutexLocker lock(&mutex_);
  const auto it = entries_.find(slot);
  if (it == entries_.end()) return 0;
  const int n = it->count;
  it->count = 0;
  return n;
}
