// Everything the operator screen shows, in one place. The screen only reads this
// and sends requests back through the invokable functions.
#pragma once

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>

#include "display_model.h"
#include "participant_model.h"
#include "slot_model.h"

class AppState : public QObject {
  Q_OBJECT
  Q_PROPERTY(SlotModel* slotList READ slotList CONSTANT)
  Q_PROPERTY(ParticipantModel* participants READ participants CONSTANT)
  Q_PROPERTY(DisplayModel* displays READ displays CONSTANT)
  Q_PROPERTY(QStringList outputChoices READ outputChoices NOTIFY changed)
  Q_PROPERTY(QStringList gridChoices READ gridChoices NOTIFY changed)
  Q_PROPERTY(int gridChoice READ gridChoice NOTIFY changed)
  Q_PROPERTY(QString meetingName MEMBER meetingName_ NOTIFY changed)
  Q_PROPERTY(bool connected MEMBER connected_ NOTIFY changed)
  Q_PROPERTY(QString elapsed MEMBER elapsed_ NOTIFY changed)
  Q_PROPERTY(QString audioDevice READ audioDevice NOTIFY changed)
  Q_PROPERTY(QString audioDeviceName MEMBER audioDeviceName_ NOTIFY changed)
  Q_PROPERTY(bool audioAsio READ audioAsio WRITE setAudioAsio NOTIFY changed)
  Q_PROPERTY(int audioChannels MEMBER audioChannels_ NOTIFY changed)
  Q_PROPERTY(int audioSampleRate MEMBER audioSampleRate_ NOTIFY changed)
  Q_PROPERTY(QString meetingId READ meetingId WRITE setMeetingId NOTIFY changed)
  Q_PROPERTY(QString joinName READ joinName WRITE setJoinName NOTIFY changed)
  Q_PROPERTY(bool qualityAuto READ qualityAuto WRITE setQualityAuto NOTIFY changed)
  Q_PROPERTY(bool cropSlides READ cropSlides WRITE setCropSlides NOTIFY changed)
  Q_PROPERTY(bool canAddSlot READ canAddSlot NOTIFY changed)
  Q_PROPERTY(bool savesSettings MEMBER savesSettings_ NOTIFY changed)
  Q_PROPERTY(QString notice MEMBER notice_ NOTIFY changed)
  Q_PROPERTY(bool audioOk MEMBER audioOk_ NOTIFY changed)
  Q_PROPERTY(bool returnVideoOk MEMBER returnVideoOk_ NOTIFY changed)
  Q_PROPERTY(bool returnAudioOk MEMBER returnAudioOk_ NOTIFY changed)
  Q_PROPERTY(bool gridMode READ gridMode WRITE setGridMode NOTIFY changed)
  Q_PROPERTY(bool demo MEMBER demo_ NOTIFY changed)
  Q_PROPERTY(int selected READ selected WRITE setSelected NOTIFY selectedChanged)
  Q_PROPERTY(int participantCount READ participantCount NOTIFY changed)
  Q_PROPERTY(int returnRow READ returnRow NOTIFY changed)
  Q_PROPERTY(QString sansFamily READ sansFamily CONSTANT)
  Q_PROPERTY(QString monoFamily READ monoFamily CONSTANT)

 public:
  explicit AppState(QObject* parent = nullptr);

  SlotModel* slotList() { return &slots_; }
  ParticipantModel* participants() { return &participants_; }
  DisplayModel* displays() { return &displays_; }

  bool gridMode() const { return gridMode_; }
  void setGridMode(bool on);
  int selected() const { return selected_; }
  void setSelected(int row);
  int participantCount() const { return static_cast<int>(participants_.rows().size()); }
  int returnRow() const { return slots_.shareCount(); }
  QString sansFamily() const { return sans_; }
  // "device (method)" for the top bar; empty when no device is chosen.
  QString audioDevice() const;
  bool audioAsio() const { return audioAsio_; }
  void setAudioAsio(bool asio);
  void setAudioDeviceName(const QString& name);
  // Slides that are not 16:9: true cuts them to 16:9, false shows all with black bars.
  bool cropSlides() const;
  void setCropSlides(bool crop);
  bool canAddSlot() const;
  // Zoom page. The meeting ID is kept as typed; "auto" picks the largest picture size
  // the number of people allows, otherwise 720p at most.
  QString meetingId() const { return meetingId_; }
  void setMeetingId(const QString& text);
  QString joinName() const { return joinName_; }
  void setJoinName(const QString& text);
  bool qualityAuto() const { return qualityAuto_; }
  void setQualityAuto(bool on);
  QStringList slotNumbers() const;
  QString monoFamily() const { return mono_; }

  // Level steps of 0.5 dB within -60..+12 dB; delay steps of 10 ms within 0..2000 ms.
  Q_INVOKABLE void nudgeGain(int row, int steps);
  Q_INVOKABLE void nudgeDelay(int row, int steps);
  Q_INVOKABLE void setFreezeOnLoss(int row, bool freeze);
  // Puts participant `participantRow` into the selected slot (replacing whoever was there).
  Q_INVOKABLE void assignToSelected(int participantRow);
  // Settings screen. Channels are 1-based and limited to the device's channel count.
  Q_INVOKABLE void toggleRoute(int row, int channel);
  Q_INVOKABLE void setLabel(int row, const QString& text);
  Q_INVOKABLE void setZoomName(int row, const QString& text);
  Q_INVOKABLE void addPersonSlot();
  // Video output page. `choice` 0 means black; choice n shows slot row n-1.
  Q_INVOKABLE void setDisplayChoice(int displayRow, int choice);
  // `choice` 0 means none; choice n is the n-th monitor that can be an output.
  Q_INVOKABLE void setGridChoice(int choice);

  // The monitors this PC has right now. `operatorKey` is the one the operator screen is on.
  struct ScreenInfo {
    QString key, title, mode;
  };
  void setScreens(const QVector<ScreenInfo>& screens, const QString& operatorKey);
  QStringList outputChoices() const;
  QStringList gridChoices() const;
  int gridChoice() const;

  // What is kept between runs: slots (names, routes, level, delay, behaviour) and the
  // audio output choice. Nothing about the current meeting.
  QJsonObject settingsJson() const;
  // Replaces the slots and audio choice. Returns false and changes nothing when the
  // object is not a settings object this program understands.
  bool applySettings(const QJsonObject& settings);
  // Four people and one slide, nobody assigned.
  void loadDefaults();

  static constexpr int kMaxPersonSlots = 16;
  static constexpr int kMaxLabelLength = 40;
  static constexpr int kMaxZoomNameLength = 100;

  // Filled by whoever supplies the data (Zoom later, the demo for now).
  QString meetingName_, elapsed_, audioDeviceName_, notice_;
  bool audioAsio_ = false;  // ASIO is not built yet; everything goes out through WASAPI
  // Shown until a device reports its real numbers.
  int audioChannels_ = 8;
  int audioSampleRate_ = 48000;
  bool savesSettings_ = false;
  bool connected_ = false, audioOk_ = false, returnVideoOk_ = false, returnAudioOk_ = false;
  bool demo_ = false;
  void notifyChanged() { emit changed(); }

 signals:
  void changed();
  // Something that settingsJson() covers was changed by the operator.
  void settingsEdited();
  // Someone was put into a slot by hand.
  void assignmentsChanged();
  void selectedChanged();

 private:
  SlotModel slots_;
  ParticipantModel participants_;
  bool gridMode_ = false;
  int selected_ = 0;
  QString sans_, mono_;

  void refreshDisplays();
  QVector<int> outputRows() const;  // display rows that can be an output

  DisplayModel displays_;
  QVector<ScreenInfo> screens_;
  QString operatorKey_;
  // Remembered by monitor name, including monitors that are unplugged right now.
  QMap<QString, int> slotByScreen_;
  QString gridScreen_;
  QString meetingId_;
  QString joinName_ = QStringLiteral("会場（backhaul）");
  bool qualityAuto_ = true;
  void edited(int row);
};
