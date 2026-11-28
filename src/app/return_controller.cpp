#include "return_controller.h"
#include "dictionary_translator.h"

#include <QImage>

#include "app_state.h"
#include "core/yuv.h"
#include "frame_store.h"

ReturnController::ReturnController(AppState& app, FrameStore& frames, std::unique_ptr<backhaul::VideoInput> input,
                                   QObject* parent)
    : QObject(parent), app_(app), frames_(frames), input_(std::move(input)) {
  asShare_ = app_.returnAsShare();
  connect(&app_, &AppState::changed, this, [this] { asShare_ = app_.returnAsShare(); });
  refreshDevices();
  timer_.setInterval(1000);
  connect(&timer_, &QTimer::timeout, this, [this] { tick(); });
  timer_.start();
  connect(&app_, &AppState::roleChanged, this, [this] {
    if (!app_.sendsVideo()) {
      stopDevice();  // a PC with another job leaves the capture device alone
    } else if (deviceIndex_ == 0) {
      startSaved();
    }
  });
}

ReturnController::~ReturnController() {
  if (input_ != nullptr) input_->stop();
}

QStringList ReturnController::deviceNames() const {
  QStringList names{T("使わない")};
  for (const backhaul::VideoDevice& d : devices_) names.push_back(QString::fromStdString(d.name));
  return names;
}

QString ReturnController::inputText() const {
  if (input_ == nullptr || deviceIndex_ == 0 || !input_->running()) return QString();
  const int milli = input_->milliFps();
  const QString rate = milli % 1000 == 0 ? QString::number(milli / 1000) : QString::number(milli / 1000.0, 'f', 2);
  return T("%1×%2・%3コマ").arg(input_->width()).arg(input_->height()).arg(rate);
}

void ReturnController::setPaused(bool paused) {
  if (paused == paused_.load()) return;
  paused_ = paused;
  emit changed();
}

void ReturnController::refreshDevices() {
  if (input_ == nullptr) return;
  const QString current = deviceIndex_ > 0 ? QString::fromStdString(devices_[static_cast<std::size_t>(deviceIndex_ - 1)].id)
                                           : QString();
  devices_ = input_->devices();
  deviceIndex_ = 0;
  for (std::size_t i = 0; i < devices_.size(); ++i) {
    if (!current.isEmpty() && QString::fromStdString(devices_[i].id) == current) deviceIndex_ = static_cast<int>(i) + 1;
  }
  emit changed();
}

void ReturnController::stopDevice() {
  if (input_ != nullptr) input_->stop();
  frames_.clear(kPreviewSlot);
  if (deviceIndex_ == 0 && problem_.isEmpty()) return;
  deviceIndex_ = 0;
  problem_.clear();
  emit changed();
}

bool ReturnController::startDevice(int index) {
  if (input_ == nullptr || index < 1 || index > static_cast<int>(devices_.size())) return false;
  if (!app_.sendsVideo()) {
    problem_ = T("このPCの役割ではZoomへ映像を送りません（役割を「返し」にすると送れます）");
    emit changed();
    return false;
  }
  std::string error;
  if (!input_->start(devices_[static_cast<std::size_t>(index - 1)].id, this, &error)) {
    deviceIndex_ = 0;
    problem_ = T("この機器から映像を取り込めませんでした：%1").arg(QString::fromStdString(error));
    frames_.clear(kPreviewSlot);
    emit changed();
    return false;
  }
  deviceIndex_ = index;
  problem_.clear();
  emit changed();
  return true;
}

void ReturnController::chooseDevice(int index) {
  if (input_ == nullptr || index < 0 || index > static_cast<int>(devices_.size())) return;
  if (index == 0) {
    stopDevice();
    app_.setVideoDeviceName(QString());
    return;
  }
  // Remember the choice even when it cannot start right now: it is what the operator wants.
  app_.setVideoDeviceName(QString::fromStdString(devices_[static_cast<std::size_t>(index - 1)].name));
  startDevice(index);
}

void ReturnController::startSaved() {
  if (input_ == nullptr || app_.videoDeviceName().isEmpty() || !app_.sendsVideo()) return;
  refreshDevices();
  for (std::size_t i = 0; i < devices_.size(); ++i) {
    if (QString::fromStdString(devices_[i].name) == app_.videoDeviceName()) {
      startDevice(static_cast<int>(i) + 1);
      return;
    }
  }
  problem_ = T("前回の機器「%1」が見つかりません").arg(app_.videoDeviceName());
  emit changed();
}

void ReturnController::reloadSaved() {
  stopDevice();
  startSaved();
}

void ReturnController::tick() {
  const int sent = sentCount_.exchange(0);
  bool moved = sent != sentPerSecond_;
  sentPerSecond_ = sent;
  // The device can disappear on its own (unplugged, signal lost and renegotiated).
  if (input_ != nullptr && deviceIndex_ > 0 && !input_->running()) {
    deviceIndex_ = 0;
    problem_ = T("映像の機器が止まりました。機器を選び直してください");
    frames_.clear(kPreviewSlot);
    moved = true;
  }
  if (moved) emit changed();
}

void ReturnController::onFrame(const std::uint8_t* y, int yStride, const std::uint8_t* uv, int uvStride, int width,
                               int height, std::int64_t timeUs) {
  // Both pacers see every picture so that switching between the two ways keeps a steady rhythm.
  const bool forCamera = cameraPacer_.accept(timeUs);
  const bool forShare = sharePacer_.accept(timeUs);
  const bool send = asShare_.load(std::memory_order_relaxed) ? forShare : forCamera;
  const bool preview = previewPacer_.accept(timeUs);
  if (!send && !preview) return;
  i420_.resize(backhaul::i420Size(width, height));
  if (i420_.empty() || !backhaul::nv12ToI420(y, yStride, uv, uvStride, width, height, i420_.data())) return;
  if (send && sender_ && !paused_.load(std::memory_order_relaxed) && sender_(i420_.data(), width, height)) {
    sentCount_.fetch_add(1, std::memory_order_relaxed);
  }
  if (preview) {
    QImage image(width, height, QImage::Format_RGB32);
    if (image.isNull() || image.bytesPerLine() != width * 4) return;
    const std::size_t w = static_cast<std::size_t>(width), h = static_cast<std::size_t>(height);
    const std::uint8_t* u = i420_.data() + w * h;
    const std::uint8_t* v = u + ((w + 1) / 2) * ((h + 1) / 2);
    backhaul::i420ToBgra(i420_.data(), u, v, width, height, true, image.bits());
    frames_.put(kPreviewSlot, image);
  }
}
