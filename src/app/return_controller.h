// The picture this PC sends into the meeting (the return role): which capture device,
// a small preview, and handing the pictures on at a steady 30 per second.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "core/nv12.h"
#include "video/video_input.h"

class AppState;
class FrameStore;

class ReturnController : public QObject, public backhaul::VideoSink {
  Q_OBJECT
  // False when this build or system has no capture devices.
  Q_PROPERTY(bool available READ available CONSTANT)
  // Entry 0 is "not used"; the others are the devices found.
  Q_PROPERTY(QStringList deviceNames READ deviceNames NOTIFY changed)
  Q_PROPERTY(int deviceIndex READ deviceIndex NOTIFY changed)
  Q_PROPERTY(QString problem READ problem NOTIFY changed)
  // "1920×1080・59.94コマ" while a device runs, otherwise empty.
  Q_PROPERTY(QString inputText READ inputText NOTIFY changed)
  // Pictures handed to the meeting during the last second.
  Q_PROPERTY(int sentPerSecond READ sentPerSecond NOTIFY changed)
  // The operator's "stop sending" switch: picture and sound stay here until switched back.
  Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY changed)

 public:
  static constexpr int kPreviewSlot = 1000;  // where the preview lives in the frame store
  static constexpr double kSendFps = 30.0;

  ReturnController(AppState& app, FrameStore& frames, std::unique_ptr<backhaul::VideoInput> input,
                   QObject* parent = nullptr);
  ~ReturnController() override;

  bool available() const { return input_ != nullptr; }
  QStringList deviceNames() const;
  int deviceIndex() const { return deviceIndex_; }
  QString problem() const { return problem_; }
  QString inputText() const;
  int sentPerSecond() const { return sentPerSecond_; }
  bool paused() const { return paused_.load(); }
  void setPaused(bool paused);

  using Sender = std::function<bool(const std::uint8_t* i420, int width, int height)>;
  // Where the pictures go. Set once, before a device is chosen; called on the device's thread.
  void setSender(Sender sender) { sender_ = std::move(sender); }

  Q_INVOKABLE void refreshDevices();
  Q_INVOKABLE void chooseDevice(int index);
  // Starts the device named in the settings, if it is present and the role sends pictures.
  void startSaved();

  // VideoSink (device thread)
  void onFrame(const std::uint8_t* y, int yStride, const std::uint8_t* uv, int uvStride, int width, int height,
               std::int64_t timeUs) override;

 signals:
  void changed();

 private:
  bool startDevice(int index);
  void stopDevice();
  void tick();

  AppState& app_;
  FrameStore& frames_;
  std::unique_ptr<backhaul::VideoInput> input_;
  std::vector<backhaul::VideoDevice> devices_;
  int deviceIndex_ = 0;
  QString problem_;
  int sentPerSecond_ = 0;
  QTimer timer_;
  Sender sender_;
  std::atomic<bool> paused_{false};
  std::atomic<int> sentCount_{0};

  // Device thread only.
  backhaul::FramePacer sendPacer_{kSendFps};
  backhaul::FramePacer previewPacer_{10.0};
  std::vector<std::uint8_t> i420_;
};
