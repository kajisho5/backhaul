#include "frame_item.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#include "core/aspect_fit.h"
#include "frame_store.h"

namespace {
FrameStore* g_store = nullptr;
}

void FrameItem::setStore(FrameStore* store) { g_store = store; }

FrameItem::FrameItem(QQuickItem* parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
  if (g_store != nullptr) {
    // Pictures arrive on another thread; the repaint request is delivered on this one.
    connect(g_store, &FrameStore::frameReady, this, [this](int slot) {
      if (slot == slot_) update();
    });
  }
}

void FrameItem::setSlot(int slot) {
  if (slot == slot_) return;
  slot_ = slot;
  emit slotChanged();
  update();
}

void FrameItem::setCrop(bool crop) {
  if (crop == crop_) return;
  crop_ = crop;
  emit cropChanged();
  update();
}

QSGNode* FrameItem::updatePaintNode(QSGNode* old, UpdatePaintNodeData*) {
  auto* node = static_cast<QSGSimpleTextureNode*>(old);
  const QImage image = g_store != nullptr ? g_store->latest(slot_) : QImage();
  const int w = static_cast<int>(width());
  const int h = static_cast<int>(height());
  if (image.isNull() || window() == nullptr || w <= 0 || h <= 0) {
    delete node;
    return nullptr;
  }
  if (node == nullptr) {
    node = new QSGSimpleTextureNode;
    node->setOwnsTexture(true);
    node->setFiltering(QSGTexture::Linear);
  }
  node->setTexture(window()->createTextureFromImage(image));
  const backhaul::FitResult fit = backhaul::fit(image.width(), image.height(), w, h,
                                          crop_ ? backhaul::FitMode::CropToFill : backhaul::FitMode::Letterbox);
  node->setSourceRect(QRectF(fit.src.x, fit.src.y, fit.src.w, fit.src.h));
  node->setRect(QRectF(fit.dst.x, fit.dst.y, fit.dst.w, fit.dst.h));
  return node;
}
