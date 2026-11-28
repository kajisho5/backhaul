// Connects a meeting (Zoom) to the application state: joining, who is there,
// which slot each person is in, and the pictures that arrive.
#pragma once

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>
#include <atomic>
#include <memory>

#include "credential_store.h"
#include "meeting/meeting_client.h"

namespace backhaul {
class AudioMixer;
}
class AppState;
class LicenceController;
class FrameStore;

class MeetingController : public QObject, public backhaul::MeetingEvents {
  Q_OBJECT
  // False when this build has no meeting library: the page then says so.
  Q_PROPERTY(bool available READ available CONSTANT)
  Q_PROPERTY(bool credentialsSupported READ credentialsSupported CONSTANT)
  Q_PROPERTY(bool credentialsSaved READ credentialsSaved NOTIFY changed)
  Q_PROPERTY(QString credentialHint READ credentialHint NOTIFY changed)
  Q_PROPERTY(QString passcode MEMBER passcode_ NOTIFY changed)
  Q_PROPERTY(QString zak MEMBER zak_ NOTIFY changed)
  // 0 nothing yet, 1 credentials accepted, 2 in the meeting, 3 receiving.
  Q_PROPERTY(int step READ step NOTIFY changed)
  Q_PROPERTY(QString joinNotice READ joinNotice NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)       // between "join" and the end of the meeting
  Q_PROPERTY(bool failed READ failed NOTIFY changed)
  Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
  Q_PROPERTY(QString detail READ detail NOTIFY changed)
  Q_PROPERTY(bool canJoin READ canJoin NOTIFY changed)
  // Sound sent into the meeting: whether the meeting takes it right now, and a note when not.
  Q_PROPERTY(bool sendingAudio READ sendingAudio NOTIFY changed)
  Q_PROPERTY(QString sendNote READ sendNote NOTIFY changed)
  Q_PROPERTY(bool sendingVideo READ sendingVideo NOTIFY changed)
  Q_PROPERTY(QString sendVideoNote READ sendVideoNote NOTIFY changed)

 public:
  MeetingController(AppState& app, FrameStore& frames, std::unique_ptr<backhaul::MeetingClient> client,
                    std::unique_ptr<CredentialStore> credentials, QObject* parent = nullptr);
  ~MeetingController() override;

  bool available() const { return client_ != nullptr; }
  bool credentialsSupported() const { return credentials_ != nullptr; }
  bool credentialsSaved() const { return !clientId_.isEmpty(); }
  QString credentialHint() const;
  int step() const;
  bool busy() const;
  bool failed() const { return state_ == backhaul::MeetingState::Failed; }
  QString statusText() const;
  QString detail() const { return detail_; }
  bool canJoin() const;
  bool sendingAudio() const { return sendingAudio_; }
  QString sendNote() const { return sendNote_; }
  bool sendingVideo() const { return sendingVideo_; }
  QString sendVideoNote() const { return sendVideoNote_; }
  // Any one thread (the capture device's thread). I420, planes packed one after another.
  bool sendVideo(const std::uint8_t* i420, int width, int height);
  // Any one thread (the sound side's sending thread). 16-bit, 48 kHz.
  bool sendAudio(const std::int16_t* samples, std::size_t frames, int channels);

  Q_INVOKABLE void join();
  // Where the Meeting SDK token and the ZAK come from when no Zoom keys are saved on this PC.
  // Not owned; must outlive this object. May be left unset.
  void setLicence(LicenceController* licence);
  // Something the operator should know about the join that is under way (may be empty).
  QString joinNotice() const { return joinNotice_; }
  Q_INVOKABLE void leave();
  Q_INVOKABLE bool saveCredentials(const QString& clientId, const QString& clientSecret);
  Q_INVOKABLE void clearCredentials();

  // MeetingEvents
  void onMeetingState(backhaul::MeetingState state, const std::string& detail) override;
  void onParticipants(const std::vector<backhaul::Participant>& everyone) override;
  void onMeetingTopic(const std::string& topic) override;
  void onShares(const std::vector<backhaul::ShareSource>& shares) override;
  void onVideoFrame(std::uint32_t userId, const backhaul::VideoFrame& frame) override;
  void onShareFrame(std::uint32_t sourceId, const backhaul::VideoFrame& frame) override;
  void onAudio(std::uint32_t userId, const std::int16_t* samples, std::size_t frames, int sampleRate,
               int channels) override;

  // Where the voices go. May be null; must outlive the meeting.
  void setMixer(backhaul::AudioMixer* mixer) { mixer_ = mixer; }

  // Digits of a meeting ID as typed ("123 4567 8901", "123-4567-8901"); 0 when it is not one.
  static quint64 parseMeetingNumber(const QString& text);

 signals:
  void changed();

 private:
  void applyParticipants();
  void applyShares();
  void putFrame(int slot, const backhaul::VideoFrame& frame);
  void updateWants();
  void everySecond();
  void resetSlots();

  AppState& app_;
  FrameStore& frames_;
  std::unique_ptr<backhaul::MeetingClient> client_;
  std::unique_ptr<CredentialStore> credentials_;
  LicenceController* licence_ = nullptr;
  bool preparing_ = false;  // waiting for the licence server before the join starts
  QString joinNotice_;
  QString clientId_, clientSecret_, passcode_, zak_, detail_;
  backhaul::MeetingState state_ = backhaul::MeetingState::Idle;
  bool authenticated_ = false;
  std::vector<backhaul::Participant> everyone_;
  std::vector<backhaul::VideoWant> wants_;
  std::vector<backhaul::ShareSource> shares_;
  // The shared screen being shown and the slot it goes to; read on the video thread.
  std::atomic<std::uint32_t> shareSource_{0};
  std::atomic<int> shareSlot_{-1};
  QTimer pollTimer_, secondTimer_;
  qint64 joinedAtMs_ = 0;
  bool sendingAudio_ = false, sendingVideo_ = false;
  QString sendNote_, sendVideoNote_;

  // Read on the video thread.
  QMutex routeMutex_;
  QHash<quint32, int> slotByUser_;
  backhaul::AudioMixer* mixer_ = nullptr;
  std::atomic<bool> audio_{true};  // false: this PC's role leaves the sound to another PC
};
