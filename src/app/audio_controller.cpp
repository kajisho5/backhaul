#include "audio_controller.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "app_state.h"

AudioController::AudioController(AppState& app, std::unique_ptr<backhaul::AudioOutput> output,
                                 std::unique_ptr<backhaul::AudioOutput> asio,
                                 std::unique_ptr<backhaul::AudioInput> input, QObject* parent)
    : QObject(parent), app_(app), output_(std::move(output)), asio_(std::move(asio)), input_(std::move(input)) {
  sink_.feed = &feed_;
  micLevels_ = {0.0, 0.0};
  if (app_.demo_) {  // made-up levels for the demo picture
    outputLevels_ = {0.62, 0.38, 0.0, 0.47, 0.0, 0.0, 0.0, 0.0};
    micLevels_ = {0.6, 0.55};
  }
  stereo_ = app_.sendStereo();
  connect(&app_, &AppState::changed, this, [this] { stereo_ = app_.sendStereo(); });
  SlotModel* list = app_.slotList();
  connect(list, &QAbstractItemModel::dataChanged, this, [this] { syncSlots(); });
  connect(list, &QAbstractItemModel::modelReset, this, [this] { syncSlots(); });
  connect(list, &QAbstractItemModel::rowsInserted, this, [this] { syncSlots(); });
  syncSlots();
  timer_.setInterval(100);
  connect(&timer_, &QTimer::timeout, this, [this] { tick(); });
  timer_.start();
  sweepTimer_.setInterval(2000);
  connect(&sweepTimer_, &QTimer::timeout, this, [this] { sweepStep(); });
  refreshDevices();
  refreshMics();
  connect(&app_, &AppState::roleChanged, this, [this] {
    stopTest();  // a test signal never outlives the screen it was started from
    if (!app_.sendsAudio()) stopMic();
    if (wantsOutput()) {
      if (deviceIndex_ == 0) startSaved();
      if (app_.sendsAudio() && micIndex_ == 0) startSavedMic();
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
    // A PC that only sends (the return role with a Windows recording device) needs no
    // output device, but its recording device still has to come back.
    if (app_.sendsAudio() && micIndex_ == 0) startSavedMic();
  });
}

AudioController::~AudioController() {
  pumpStop_ = true;
  if (pumpThread_.joinable()) pumpThread_.join();
  if (asio_ != nullptr) asio_->setInputSink(nullptr);
  if (input_ != nullptr) input_->stop();
  if (output_ != nullptr) output_->stop();
  if (asio_ != nullptr) asio_->stop();
}

// The output device runs on a PC that plays the meeting's voices, and also on a PC that
// only sends when its inputs are ASIO inputs (they belong to the running ASIO device).
bool AudioController::wantsOutput() const {
  return app_.playsAudio() || (app_.sendsAudio() && app_.audioAsio());
}

backhaul::AudioOutput* AudioController::active() const {
  return app_.audioAsio() ? asio_.get() : output_.get();
}

void AudioController::setAsio(bool asio) {
  if (asio == app_.audioAsio() || (asio && asio_ == nullptr)) return;
  stopTest();
  stopMic();
  app_.setMicDeviceName(QString());  // the two methods name their inputs differently
  if (active() != nullptr) active()->stop();
  devices_.clear();
  deviceIndex_ = 0;
  problem_.clear();
  app_.audioOk_ = false;
  app_.setAudioDeviceName(QString());
  app_.setAudioAsio(asio);
  refreshDevices();
  refreshMics();
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
  if (!wantsOutput()) {
    problem_ = QStringLiteral("このPCの役割では音を出しません（役割を「音声」か「1台でぜんぶ」にすると出せます）");
    emit changed();
    return false;
  }
  stopTest();  // another device, other speakers: the operator starts the check again
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
  if (app_.audioAsio()) {  // the ASIO inputs belong to the device that just started
    refreshMics();
    if (app_.sendsAudio()) startSavedMic();
  }
  return true;
}

void AudioController::chooseDevice(int index) {
  if (active() == nullptr || index < 0 || index > static_cast<int>(devices_.size())) return;
  if (index == 0) {
    stopTest();
    if (app_.audioAsio()) stopMic();
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
  if (!app_.audioAsio() && app_.sendsAudio() && micIndex_ == 0) startSavedMic();
  if (app_.audioDeviceName_.isEmpty() || !wantsOutput()) return;
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

void AudioController::reloadSaved() {
  stopTest();
  stopMic();
  if (output_ != nullptr) output_->stop();
  if (asio_ != nullptr) asio_->stop();
  deviceIndex_ = 0;
  problem_.clear();
  app_.audioOk_ = false;
  stereo_ = app_.sendStereo();
  syncSlots();
  refreshDevices();
  refreshMics();
  app_.notifyChanged();
  emit changed();
  startSaved();
}

// ---- recording side ----

bool AudioController::micAvailable() const { return app_.audioAsio() ? asio_ != nullptr : input_ != nullptr; }

QStringList AudioController::micNames() const {
  QStringList names{QStringLiteral("使わない")};
  if (app_.audioAsio()) {
    for (int i = 0; i < micPairs_; ++i) names.push_back(QStringLiteral("入力 %1-%2").arg(i * 2 + 1).arg(i * 2 + 2));
  } else {
    for (const backhaul::AudioDevice& d : mics_) names.push_back(QString::fromStdString(d.name));
  }
  return names;
}

void AudioController::refreshMics() {
  if (app_.audioAsio()) {
    mics_.clear();
    const int inputs = asio_ != nullptr && asio_->running() ? asio_->inputChannels() : 0;
    micPairs_ = (inputs + 1) / 2;
    if (micIndex_ > micPairs_) stopMic();
  } else {
    micPairs_ = 0;
    const QString current = micIndex_ > 0 && micIndex_ <= static_cast<int>(mics_.size())
                                ? QString::fromStdString(mics_[static_cast<std::size_t>(micIndex_ - 1)].id)
                                : QString();
    mics_ = input_ != nullptr ? input_->devices() : std::vector<backhaul::AudioDevice>{};
    micIndex_ = 0;
    for (std::size_t i = 0; i < mics_.size(); ++i) {
      if (!current.isEmpty() && QString::fromStdString(mics_[i].id) == current) micIndex_ = static_cast<int>(i) + 1;
    }
  }
  emit changed();
}

void AudioController::stopMic() {
  if (asio_ != nullptr) asio_->setInputSink(nullptr);
  if (input_ != nullptr) input_->stop();
  if (micIndex_ == 0 && micProblem_.isEmpty()) return;
  micIndex_ = 0;
  micProblem_.clear();
  emit changed();
}

bool AudioController::startMic(int index) {
  if (!app_.sendsAudio()) {
    micProblem_ = QStringLiteral("このPCの役割ではZoomへ音を送りません（役割を「音声」にすると送れます）");
    emit changed();
    return false;
  }
  if (app_.audioAsio()) {
    if (asio_ == nullptr || !asio_->running() || index < 1 || index > micPairs_) {
      micIndex_ = 0;
      micProblem_ = QStringLiteral("ASIOの入力は、スピーカー側の機器を選んでから選べます");
      emit changed();
      return false;
    }
    sink_.firstChannel = (index - 1) * 2;
    asio_->setInputSink(&sink_);
  } else {
    if (input_ == nullptr || index < 1 || index > static_cast<int>(mics_.size())) return false;
    sink_.firstChannel = 0;
    std::string error;
    if (!input_->start(mics_[static_cast<std::size_t>(index - 1)].id, &sink_, &error)) {
      micIndex_ = 0;
      micProblem_ = QStringLiteral("この機器から音を取り込めませんでした：%1").arg(QString::fromStdString(error));
      emit changed();
      return false;
    }
  }
  micIndex_ = index;
  micProblem_.clear();
  if (!pumpThread_.joinable()) pumpThread_ = std::thread([this] { pump(); });
  emit changed();
  return true;
}

void AudioController::chooseMic(int index) {
  const int count = static_cast<int>(micNames().size()) - 1;
  if (!micAvailable() || index < 0 || index > count) return;
  if (index == 0) {
    stopMic();
    app_.setMicDeviceName(QString());
    return;
  }
  // Remember the choice even when it cannot start right now: it is what the operator wants.
  app_.setMicDeviceName(micNames().at(index));
  startMic(index);
}

void AudioController::startSavedMic() {
  if (app_.micDeviceName().isEmpty() || !app_.sendsAudio() || !micAvailable()) return;
  if (!app_.audioAsio()) refreshMics();
  const int index = static_cast<int>(micNames().indexOf(app_.micDeviceName()));
  if (index >= 1) {
    startMic(index);
    return;
  }
  // With ASIO the inputs appear once the output device runs; that start calls this again.
  if (app_.audioAsio() && (asio_ == nullptr || !asio_->running())) return;
  micProblem_ = QStringLiteral("前回の入力「%1」が見つかりません").arg(app_.micDeviceName());
  emit changed();
}

void AudioController::setSender(Sender sender) { sender_ = std::move(sender); }

int AudioController::pumpOnce() {
  constexpr std::size_t kBlock = 480;  // 10 ms
  float left[kBlock], right[kBlock];
  std::int16_t block[kBlock * 2];
  int blocks = 0;
  while (feed_.available() >= kBlock) {
    const bool stereo = stereo_.load(std::memory_order_relaxed);
    const std::size_t frames = feed_.readFloat(left, right, kBlock);
    float* const both[2] = {left, right};
    if (frames > 0) micEffects_.process(both, 2, frames);
    backhaul::MicFeed::toPcm(left, right, frames, stereo, block);
    // When the meeting is not taking sound the block is simply let go, so nothing piles up.
    if (frames > 0 && sender_) sender_(block, frames, stereo ? 2 : 1);
    ++blocks;
  }
  return blocks;
}

void AudioController::pump() {
  while (!pumpStop_.load(std::memory_order_relaxed)) {
    if (pumpOnce() == 0) std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

// ---- sound check ----

int AudioController::testChannels() const {
  if (app_.demo_) return static_cast<int>(outputLevels_.size());  // the demo picture has no device
  return deviceIndex_ > 0 ? std::min(app_.audioChannels_, backhaul::AudioMixer::kMaxChannels) : 0;
}

QVariantList AudioController::testOn() const {
  QVariantList on;
  for (int c = 0; c < testChannels(); ++c) on.push_back((testMask_ & (std::uint32_t{1} << c)) != 0);
  return on;
}

QString AudioController::testChannelsText() const {
  QStringList numbers;
  for (int c = 0; c < backhaul::AudioMixer::kMaxChannels; ++c) {
    if ((testMask_ & (std::uint32_t{1} << c)) != 0) numbers.push_back(QString::number(c + 1));
  }
  return numbers.join(QStringLiteral(", "));
}

void AudioController::applyTest(std::uint32_t mask) {
  testMask_ = mask;
  mixer_.tone().setChannels(mask);
  emit testChanged();
}

void AudioController::toggleTest(int channel) {
  if (channel < 0 || channel >= testChannels()) return;
  const std::uint32_t bit = std::uint32_t{1} << channel;
  if (sweepChannel_ >= 0) {  // taking over by hand: the sweep ends, this channel is what was asked for
    sweepTimer_.stop();
    sweepChannel_ = -1;
    applyTest(bit);
    return;
  }
  applyTest(testMask_ ^ bit);
}

void AudioController::stopTest() {
  if (testMask_ == 0 && sweepChannel_ < 0) return;
  sweepTimer_.stop();
  sweepChannel_ = -1;
  applyTest(0);
}

void AudioController::sweepTest() {
  if (testChannels() < 1) return;
  sweepChannel_ = 0;
  sweepTimer_.start();
  applyTest(1);
}

void AudioController::sweepStep() {
  if (sweepChannel_ < 0) return;
  if (sweepChannel_ + 1 >= testChannels()) {
    stopTest();
    return;
  }
  ++sweepChannel_;
  applyTest(std::uint32_t{1} << sweepChannel_);
}

void AudioController::setTestKind(int kind) {
  if (kind < 0 || kind > 1 || kind == testKind_) return;
  testKind_ = kind;
  mixer_.tone().setKind(kind == 1 ? backhaul::TestTone::Kind::Pink : backhaul::TestTone::Kind::Sine);
  emit testChanged();
}

void AudioController::nudgeTestLevel(int direction) {
  const int next = std::clamp(testLevelDb_ + (direction > 0 ? 2 : -2), testLevelMin(), testLevelMax());
  if (next == testLevelDb_) return;
  testLevelDb_ = next;
  mixer_.tone().setLevelDb(static_cast<float>(next));
  emit testChanged();
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
  auto shownLevel = [](float peak, double before) {
    const double db = peak > 0.0f ? 20.0 * std::log10(static_cast<double>(peak)) : -120.0;
    return std::max(std::clamp((db + 60.0) / 60.0, 0.0, 1.0), before - 0.08);
  };
  const int outputs = deviceIndex_ > 0 ? std::min(app_.audioChannels_, backhaul::AudioMixer::kMaxChannels) : 0;
  QVariantList out;
  bool moved = outputs != outputLevels_.size();
  for (int c = 0; c < outputs; ++c) {
    const double before = c < outputLevels_.size() ? outputLevels_[c].toDouble() : 0.0;
    const double level = shownLevel(mixer_.takeOutputPeak(c), before);
    moved = moved || std::fabs(level - before) > 0.004;
    out.push_back(level);
  }
  QVariantList mic;
  for (int side = 0; side < 2; ++side) {
    const double before = micLevels_[side].toDouble();
    const double level = micIndex_ > 0 ? shownLevel(feed_.takePeak(side), before) : 0.0;
    moved = moved || std::fabs(level - before) > 0.004;
    mic.push_back(level);
  }
  if (moved) {
    outputLevels_ = out;
    micLevels_ = mic;
    emit levelsChanged();
  }
  // A recording device can disappear on its own too.
  if (micIndex_ > 0 && !app_.audioAsio() && input_ != nullptr && !input_->running()) {
    micIndex_ = 0;
    micProblem_ = QStringLiteral("入力の機器が止まりました。機器を選び直してください");
    emit changed();
  }
  // The device can disappear on its own (unplugged, driver restarted).
  if (active() != nullptr && deviceIndex_ > 0 && !active()->running()) {
    stopTest();
    deviceIndex_ = 0;
    problem_ = QStringLiteral("音声の機器が止まりました。機器を選び直してください");
    app_.audioOk_ = false;
    app_.notifyChanged();
    emit changed();
  }
}
