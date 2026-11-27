// The sound side of the application: which device plays, and keeping the mixer in
// step with the slots (level, delay, channel assignment, meters).
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "audio/audio_output.h"
#include "core/audio_mixer.h"
#include "core/effect_chain.h"
#include "core/mic_feed.h"

class AppState;

class AudioController : public QObject {
  Q_OBJECT
  // False when this build or system has no sound output (the page then only shows the saved name).
  Q_PROPERTY(bool available READ available CONSTANT)
  // False when the program was built without the ASIO SDK: only the system output is offered then.
  Q_PROPERTY(bool asioAvailable READ asioAvailable CONSTANT)
  // Entry 0 is "no output"; the others are the devices found.
  Q_PROPERTY(QStringList deviceNames READ deviceNames NOTIFY changed)
  Q_PROPERTY(int deviceIndex READ deviceIndex NOTIFY changed)
  Q_PROPERTY(QString problem READ problem NOTIFY changed)
  // Level of each output channel of the running device, 0..1 (same scale as the slot meters).
  Q_PROPERTY(QVariantList outputLevels READ outputLevels NOTIFY levelsChanged)
  // Recording side (sound sent into the meeting). Entry 0 is "not used". With ASIO the
  // entries are input pairs of the running output device; otherwise Windows recording devices.
  Q_PROPERTY(bool micAvailable READ micAvailable NOTIFY changed)
  Q_PROPERTY(QStringList micNames READ micNames NOTIFY changed)
  Q_PROPERTY(int micIndex READ micIndex NOTIFY changed)
  Q_PROPERTY(QString micProblem READ micProblem NOTIFY changed)
  Q_PROPERTY(QVariantList micLevels READ micLevels NOTIFY levelsChanged)  // left, right
  // Sound check: a test signal on chosen output channels. Never part of what goes to the meeting.
  Q_PROPERTY(QVariantList testOn READ testOn NOTIFY testChanged)  // one true/false per output channel
  Q_PROPERTY(bool testing READ testing NOTIFY testChanged)        // at least one channel sounds
  Q_PROPERTY(bool testSweeping READ testSweeping NOTIFY testChanged)
  Q_PROPERTY(QString testChannelsText READ testChannelsText NOTIFY testChanged)  // "1, 3"
  Q_PROPERTY(int testKind READ testKind WRITE setTestKind NOTIFY testChanged)    // 0 = 1 kHz sine, 1 = pink noise
  Q_PROPERTY(int testLevelDb READ testLevelDb NOTIFY testChanged)
  Q_PROPERTY(int testLevelMin READ testLevelMin CONSTANT)
  Q_PROPERTY(int testLevelMax READ testLevelMax CONSTANT)

 public:
  static constexpr int kSources = 20;  // every possible slot: 16 people and 4 slides

  // `output` is the system's own output; `asio` may be null.
  // `input` is the system's recording side; it may be null like the others.
  AudioController(AppState& app, std::unique_ptr<backhaul::AudioOutput> output,
                  std::unique_ptr<backhaul::AudioOutput> asio = nullptr,
                  std::unique_ptr<backhaul::AudioInput> input = nullptr, QObject* parent = nullptr);
  ~AudioController() override;

  backhaul::AudioMixer* mixer() { return &mixer_; }
  bool available() const { return output_ != nullptr || asio_ != nullptr; }
  bool asioAvailable() const { return asio_ != nullptr; }
  QStringList deviceNames() const;
  int deviceIndex() const { return deviceIndex_; }
  QString problem() const { return problem_; }
  QVariantList outputLevels() const { return outputLevels_; }
  bool micAvailable() const;
  QStringList micNames() const;
  int micIndex() const { return micIndex_; }
  QString micProblem() const { return micProblem_; }
  QVariantList micLevels() const { return micLevels_; }
  QVariantList testOn() const;
  bool testing() const { return testMask_ != 0; }
  bool testSweeping() const { return sweepChannel_ >= 0; }
  QString testChannelsText() const;
  int testKind() const { return testKind_; }
  void setTestKind(int kind);
  int testLevelDb() const { return testLevelDb_; }
  int testLevelMin() const { return static_cast<int>(backhaul::TestTone::kMinDb); }
  int testLevelMax() const { return static_cast<int>(backhaul::TestTone::kMaxDb); }

  using Sender = std::function<bool(const std::int16_t* samples, std::size_t frames, int channels)>;
  // Where recorded sound goes, in blocks of 10 ms, on a thread of this object.
  // Set once, before a recording device is chosen; must stay valid until this object is gone.
  void setSender(Sender sender);
  // The sending thread's work, also callable from a test: hands over every full 10 ms block
  // that is waiting. Returns the number of blocks handed over.
  int pumpOnce();
  backhaul::MicFeed* micFeed() { return &feed_; }
  // Effects the recorded sound goes through before it is sent (two channels). May be null.
  // Set once, before a recording device is chosen; must stay valid until this object is gone.
  void setMicEffects(backhaul::EffectChain* effects) { micEffects_ = effects; }

  Q_INVOKABLE void refreshDevices();
  Q_INVOKABLE void chooseDevice(int index);
  // Switches between ASIO and the system output. The device has to be chosen again afterwards.
  Q_INVOKABLE void setAsio(bool asio);
  Q_INVOKABLE void chooseMic(int index);
  // Reads the list of recording devices again (something was plugged in).
  Q_INVOKABLE void refreshMics();

  // Sound check. Channels count from 0. Switching a channel by hand ends a running sweep.
  Q_INVOKABLE void toggleTest(int channel);
  Q_INVOKABLE void stopTest();
  // One channel after the other, from the first to the last, then silence.
  Q_INVOKABLE void sweepTest();
  Q_INVOKABLE void nudgeTestLevel(int direction);  // one step is 2 dB
  // The sweep's clock, also callable from a test: moves on to the next channel.
  void sweepStep();

  // Starts the device named in the settings, if it is present. Call once after loading settings.
  void startSaved();

 signals:
  void changed();
  void levelsChanged();
  void testChanged();

 private:
  void syncSlots();
  void tick();
  bool startDevice(int index);
  bool wantsOutput() const;
  bool startMic(int index);
  void stopMic();
  void startSavedMic();
  void pump();
  int testChannels() const;  // how many output channels can be checked right now
  void applyTest(std::uint32_t mask);

  // Recorded sound arrives here on a device thread.
  struct Sink final : backhaul::AudioSink {
    backhaul::MicFeed* feed = nullptr;
    std::atomic<int> firstChannel{0};
    void onCaptured(const float* interleaved, std::size_t frames, int channels) override {
      feed->write(interleaved, frames, channels, firstChannel.load(std::memory_order_relaxed));
    }
  };

  AppState& app_;
  backhaul::AudioMixer mixer_{kSources};
  std::unique_ptr<backhaul::AudioOutput> output_;
  std::unique_ptr<backhaul::AudioOutput> asio_;
  backhaul::AudioOutput* active() const;
  std::vector<backhaul::AudioDevice> devices_;
  int deviceIndex_ = 0;
  QString problem_;
  QTimer timer_;
  QVariantList outputLevels_;
  std::uint32_t testMask_ = 0;
  int testKind_ = 0;
  int testLevelDb_ = static_cast<int>(backhaul::TestTone::kDefaultDb);
  int sweepChannel_ = -1;  // -1 = no sweep running
  QTimer sweepTimer_;

  std::unique_ptr<backhaul::AudioInput> input_;
  std::vector<backhaul::AudioDevice> mics_;  // system recording devices (not used with ASIO)
  int micPairs_ = 0;                         // ASIO: input pairs of the running device
  int micIndex_ = 0;
  QString micProblem_;
  QVariantList micLevels_;
  backhaul::MicFeed feed_;
  Sink sink_;
  Sender sender_;
  backhaul::EffectChain* micEffects_ = nullptr;
  std::atomic<bool> stereo_{false};
  std::atomic<bool> pumpStop_{false};
  std::thread pumpThread_;
};
