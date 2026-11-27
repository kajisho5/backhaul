// The sound side of the application: which device plays, and keeping the mixer in
// step with the slots (level, delay, channel assignment, meters).
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <memory>
#include <vector>

#include "audio/audio_output.h"
#include "core/audio_mixer.h"

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

 public:
  static constexpr int kSources = 20;  // every possible slot: 16 people and 4 slides

  // `output` is the system's own output; `asio` may be null.
  AudioController(AppState& app, std::unique_ptr<backhaul::AudioOutput> output,
                  std::unique_ptr<backhaul::AudioOutput> asio = nullptr, QObject* parent = nullptr);
  ~AudioController() override;

  backhaul::AudioMixer* mixer() { return &mixer_; }
  bool available() const { return output_ != nullptr || asio_ != nullptr; }
  bool asioAvailable() const { return asio_ != nullptr; }
  QStringList deviceNames() const;
  int deviceIndex() const { return deviceIndex_; }
  QString problem() const { return problem_; }

  Q_INVOKABLE void refreshDevices();
  Q_INVOKABLE void chooseDevice(int index);
  // Switches between ASIO and the system output. The device has to be chosen again afterwards.
  Q_INVOKABLE void setAsio(bool asio);

  // Starts the device named in the settings, if it is present. Call once after loading settings.
  void startSaved();

 signals:
  void changed();

 private:
  void syncSlots();
  void tick();
  bool startDevice(int index);

  AppState& app_;
  backhaul::AudioMixer mixer_{kSources};
  std::unique_ptr<backhaul::AudioOutput> output_;
  std::unique_ptr<backhaul::AudioOutput> asio_;
  backhaul::AudioOutput* active() const;
  std::vector<backhaul::AudioDevice> devices_;
  int deviceIndex_ = 0;
  QString problem_;
  QTimer timer_;
};
