#include "audio_controller.h"

#include <algorithm>
#include <cmath>

#include "app_state.h"

AudioController::AudioController(AppState& app, std::unique_ptr<backhaul::AudioOutput> output,
                                 std::unique_ptr<backhaul::AudioOutput> asio, QObject* parent)
    : QObject(parent), app_(app), output_(std::move(output)), asio_(std::move(asio)) {
  SlotModel* list = app_.slotList();
  connect(list, &QAbstractItemModel::dataChanged, this, [this] { syncSlots(); });
  connect(list, &QAbstractItemModel::modelReset, this, [this] { syncSlots(); });
  connect(list, &QAbstractItemModel::rowsInserted, this, [this] { syncSlots(); });
  syncSlots();
  timer_.setInterval(100);
  connect(&timer_, &QTimer::timeout, this, [this] { tick(); });
  timer_.start();
  refreshDevices();
  connect(&app_, &AppState::roleChanged, this, [this] {
    if (app_.handlesAudio()) {
      if (deviceIndex_ == 0) startSaved();
      return;
    }
    // This PC's role leaves the sound to another PC: nothing may come out here.
    if (output_ != nullptr) output_->stop();
    if (asio_ != nullptr) asio_->stop();
    deviceIndex_ = 0;
    problem_.clear();
    app_.audioOk_ = false;
    app_.notifyChanged();
    emit changed();
  });
}

AudioController::~AudioController() {
  if (output_ != nullptr) output_->stop();
  if (asio_ != nullptr) asio_->stop();
}

backhaul::AudioOutput* AudioController::active() const {
  return app_.audioAsio() ? asio_.get() : output_.get();
}

void AudioController::setAsio(bool asio) {
  if (asio == app_.audioAsio() || (asio && asio_ == nullptr)) return;
  if (active() != nullptr) active()->stop();
  devices_.clear();
  deviceIndex_ = 0;
  problem_.clear();
  app_.audioOk_ = false;
  app_.setAudioDeviceName(QString());
  app_.setAudioAsio(asio);
  refreshDevices();
}

QStringList AudioController::deviceNames() const {
  QStringList names{QStringLiteral("出さない")};
  for (const backhaul::AudioDevice& d : devices_) names.push_back(QString::fromStdString(d.name));
  return names;
}

void AudioController::refreshDevices() {
  if (active() == nullptr) {
    devices_.clear();
    deviceIndex_ = 0;
    emit changed();
    return;
  }
  const QString current = deviceIndex_ > 0 ? QString::fromStdString(devices_[static_cast<std::size_t>(deviceIndex_ - 1)].id) : QString();
  devices_ = active()->devices();
  deviceIndex_ = 0;
  for (std::size_t i = 0; i < devices_.size(); ++i) {
    if (!current.isEmpty() && QString::fromStdString(devices_[i].id) == current) deviceIndex_ = static_cast<int>(i) + 1;
  }
  emit changed();
}

bool AudioController::startDevice(int index) {
  if (active() == nullptr || index < 1 || index > static_cast<int>(devices_.size())) return false;
  if (!app_.handlesAudio()) {
    problem_ = QStringLiteral("このPCの役割では音を出しません（役割を「1台でぜんぶ」にすると出せます）");
    emit changed();
    return false;
  }
  const backhaul::AudioDevice& device = devices_[static_cast<std::size_t>(index - 1)];
  std::string error;
  if (!active()->start(device.id, &mixer_, &error)) {
    deviceIndex_ = 0;
    problem_ = QStringLiteral("この機器から音を出せませんでした：%1").arg(QString::fromStdString(error));
    app_.audioOk_ = false;
    app_.notifyChanged();
    emit changed();
    return false;
  }
  deviceIndex_ = index;
  problem_.clear();
  app_.audioOk_ = true;
  app_.audioChannels_ = std::min(active()->channels(), backhaul::AudioMixer::kMaxChannels);
  app_.audioSampleRate_ = backhaul::AudioMixer::kRate;
  app_.notifyChanged();
  emit changed();
  return true;
}

void AudioController::chooseDevice(int index) {
  if (active() == nullptr || index < 0 || index > static_cast<int>(devices_.size())) return;
  if (index == 0) {
    active()->stop();
    deviceIndex_ = 0;
    problem_.clear();
    app_.audioOk_ = false;
    app_.setAudioDeviceName(QString());
    app_.notifyChanged();
    emit changed();
    return;
  }
  // Remember the choice even when it cannot start right now: it is what the operator wants.
  app_.setAudioDeviceName(QString::fromStdString(devices_[static_cast<std::size_t>(index - 1)].name));
  startDevice(index);
}

void AudioController::startSaved() {
  if (app_.audioDeviceName_.isEmpty() || !app_.handlesAudio()) return;
  if (active() == nullptr) {
    problem_ = app_.audioAsio() ? QStringLiteral("このプログラムにはASIOが入っていません。方式を選び直してください")
                                : QStringLiteral("このPCでは音を出せません");
    emit changed();
    return;
  }
  refreshDevices();
  for (std::size_t i = 0; i < devices_.size(); ++i) {
    if (QString::fromStdString(devices_[i].name) == app_.audioDeviceName_) {
      startDevice(static_cast<int>(i) + 1);
      return;
    }
  }
  problem_ = QStringLiteral("前回の機器「%1」が見つかりません").arg(app_.audioDeviceName_);
  emit changed();
}

void AudioController::syncSlots() {
  const auto& rows = app_.slotList()->rows();
  for (int i = 0; i < kSources; ++i) {
    if (i < rows.size()) {
      mixer_.setGainDb(i, static_cast<float>(rows[i].gainDb));
      mixer_.setDelayMs(i, rows[i].delayMs);
      mixer_.setChannels(i, rows[i].channels);
    } else {
      mixer_.setChannels(i, 0);
    }
  }
}

void AudioController::tick() {
  if (app_.demo_) return;  // made-up levels stay as they are
  // Meters: a quick rise and a slower fall, on a scale where 1.0 is full level and 0 is -60 dB.
  const auto& rows = app_.slotList()->rows();
  for (int i = 0; i < rows.size() && i < kSources; ++i) {
    const float peak = mixer_.takePeak(i);
    const double db = peak > 0.0f ? 20.0 * std::log10(static_cast<double>(peak)) : -120.0;
    const double shown = std::clamp((db + 60.0) / 60.0, 0.0, 1.0);
    const double level = std::max(shown, rows[i].level - 0.08);
    if (std::fabs(level - rows[i].level) > 0.004) app_.slotList()->setLevel(i, level);
  }
  // The device can disappear on its own (unplugged, driver restarted).
  if (active() != nullptr && deviceIndex_ > 0 && !active()->running()) {
    deviceIndex_ = 0;
    problem_ = QStringLiteral("音声の機器が止まりました。機器を選び直してください");
    app_.audioOk_ = false;
    app_.notifyChanged();
    emit changed();
  }
}
