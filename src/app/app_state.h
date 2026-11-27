// Everything the operator screen shows, in one place. The screen only reads this
// and sends requests back through the invokable functions.
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

#include "participant_model.h"
#include "slot_model.h"

class AppState : public QObject {
  Q_OBJECT
  Q_PROPERTY(SlotModel* slotList READ slotList CONSTANT)
  Q_PROPERTY(ParticipantModel* participants READ participants CONSTANT)
  Q_PROPERTY(QString meetingName MEMBER meetingName_ NOTIFY changed)
  Q_PROPERTY(bool connected MEMBER connected_ NOTIFY changed)
  Q_PROPERTY(QString elapsed MEMBER elapsed_ NOTIFY changed)
  Q_PROPERTY(QString audioDevice READ audioDevice NOTIFY changed)
  Q_PROPERTY(QString audioDeviceName MEMBER audioDeviceName_ NOTIFY changed)
  Q_PROPERTY(bool audioAsio READ audioAsio WRITE setAudioAsio NOTIFY changed)
  Q_PROPERTY(int audioChannels MEMBER audioChannels_ NOTIFY changed)
  Q_PROPERTY(int audioSampleRate MEMBER audioSampleRate_ NOTIFY changed)
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
  // Slides that are not 16:9: true cuts them to 16:9, false shows all with black bars.
  bool cropSlides() const;
  void setCropSlides(bool crop);
  bool canAddSlot() const;
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
  bool audioAsio_ = true;
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
  void selectedChanged();

 private:
  SlotModel slots_;
  ParticipantModel participants_;
  bool gridMode_ = false;
  int selected_ = 0;
  QString sans_, mono_;

  QStringList slotNumbers() const;
  void edited(int row);
};
