// Everything the operator screen shows, in one place. The screen only reads this
// and sends requests back through the invokable functions.
#pragma once

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
  Q_PROPERTY(QString audioDevice MEMBER audioDevice_ NOTIFY changed)
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
  QString monoFamily() const { return mono_; }

  // Level steps of 0.5 dB within -60..+12 dB; delay steps of 10 ms within 0..2000 ms.
  Q_INVOKABLE void nudgeGain(int row, int steps);
  Q_INVOKABLE void nudgeDelay(int row, int steps);
  Q_INVOKABLE void setFreezeOnLoss(int row, bool freeze);
  // Puts participant `participantRow` into the selected slot (replacing whoever was there).
  Q_INVOKABLE void assignToSelected(int participantRow);

  // Filled by whoever supplies the data (Zoom later, the demo for now).
  QString meetingName_, elapsed_, audioDevice_;
  bool connected_ = false, audioOk_ = false, returnVideoOk_ = false, returnAudioOk_ = false;
  bool demo_ = false;
  void notifyChanged() { emit changed(); }

 signals:
  void changed();
  void selectedChanged();

 private:
  SlotModel slots_;
  ParticipantModel participants_;
  bool gridMode_ = false;
  int selected_ = 0;
  QString sans_, mono_;
};
