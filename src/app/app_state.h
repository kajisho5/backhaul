// Everything the operator screen shows, in one place. The screen only reads this
// and sends requests back through the invokable functions.
#pragma once

#include <QJsonObject>
#include "dictionary_translator.h"
#include <QMap>
#include <QObject>
#include <QString>

#include "core/role.h"
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
  // Sound sent into the meeting (the sound role): the recording device by name, how the
  // meeting treats it (0 speech, 1 original, 2 music) and whether both sides are sent.
  Q_PROPERTY(QString micDeviceName READ micDeviceName NOTIFY changed)
  Q_PROPERTY(int sendQuality READ sendQuality WRITE setSendQuality NOTIFY changed)
  Q_PROPERTY(bool sendStereo READ sendStereo WRITE setSendStereo NOTIFY changed)
  Q_PROPERTY(bool sendsAudio READ sendsAudio NOTIFY roleChanged)
  Q_PROPERTY(bool playsAudio READ playsAudio NOTIFY roleChanged)
  // Picture sent into the meeting (the return role): the capture device by name, and
  // whether it goes out as a shared screen instead of as this PC's camera.
  Q_PROPERTY(QString videoDeviceName READ videoDeviceName NOTIFY changed)
  Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
  Q_PROPERTY(bool returnAsShare READ returnAsShare WRITE setReturnAsShare NOTIFY changed)
  Q_PROPERTY(bool returnInAll READ returnInAll WRITE setReturnInAll NOTIFY roleChanged)
  // Whether the program looks for a newer version when it starts.
  Q_PROPERTY(bool checksUpdates READ checksUpdates WRITE setChecksUpdates NOTIFY changed)
  // Whether this PC joins the meeting again by itself after the line dropped.
  Q_PROPERTY(bool rejoins READ rejoins WRITE setRejoins NOTIFY changed)
  Q_PROPERTY(bool sendsVideo READ sendsVideo NOTIFY roleChanged)
  Q_PROPERTY(bool receivesVideo READ receivesVideo NOTIFY roleChanged)
  Q_PROPERTY(int audioChannels MEMBER audioChannels_ NOTIFY changed)
  Q_PROPERTY(int audioSampleRate MEMBER audioSampleRate_ NOTIFY changed)
  Q_PROPERTY(QString meetingId READ meetingId WRITE setMeetingId NOTIFY changed)
  Q_PROPERTY(QString joinName READ joinName WRITE setJoinName NOTIFY changed)
  Q_PROPERTY(bool qualityAuto READ qualityAuto WRITE setQualityAuto NOTIFY changed)
  Q_PROPERTY(bool cropSlides READ cropSlides WRITE setCropSlides NOTIFY changed)
  // Shown on the slide output while nobody has shared yet; empty means plain black.
  Q_PROPERTY(QString standbyText READ standbyText WRITE setStandbyText NOTIFY changed)
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
  // What this PC is used for: "" (not chosen yet), "all", "camera" or "twoshot".
  Q_PROPERTY(QString role READ role WRITE setRole NOTIFY roleChanged)
  Q_PROPERTY(bool handlesAudio READ handlesAudio NOTIFY roleChanged)
  // Single-output roles: the slot row whose picture goes out (the first person, or the slide).
  Q_PROPERTY(int roleRow READ roleRow NOTIFY changed)
  // Single-output roles: the monitor the role's picture goes to (0 = none).
  Q_PROPERTY(QStringList roleOutputChoices READ gridChoices NOTIFY changed)
  Q_PROPERTY(int roleOutputChoice READ roleOutputChoice NOTIFY changed)
  // Two-shot: slot rows shown left and right, how they are fitted, and which halves are drawn.
  Q_PROPERTY(int twoShotLeft READ twoShotLeft NOTIFY changed)
  Q_PROPERTY(int twoShotRight READ twoShotRight NOTIFY changed)
  Q_PROPERTY(bool twoShotFill READ twoShotFill WRITE setTwoShotFill NOTIFY changed)
  Q_PROPERTY(bool twoShotSolo READ twoShotSolo WRITE setTwoShotSolo NOTIFY changed)
  // For the two pickers: entry 0 is "nobody", then everyone in the meeting except this PC.
  Q_PROPERTY(QStringList pickNames READ pickNames NOTIFY changed)
  Q_PROPERTY(int twoShotLeftPick READ twoShotLeftPick NOTIFY changed)
  Q_PROPERTY(int twoShotRightPick READ twoShotRightPick NOTIFY changed)
  Q_PROPERTY(int twoShotSplit READ twoShotSplit NOTIFY changed)  // 0 both, 1 left only, 2 right only
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
  QString micDeviceName() const { return micDeviceName_; }
  void setMicDeviceName(const QString& name);
  int sendQuality() const { return sendQuality_; }
  void setSendQuality(int quality);
  // Stereo needs "original" or "music": with speech processing the meeting sends one side only.
  bool sendStereo() const { return sendStereo_ && sendQuality_ != 0; }
  void setSendStereo(bool stereo);
  bool sendsAudio() const { return backhaul::sendsAudio(role_, returnInAll_); }
  bool playsAudio() const { return backhaul::playsAudio(role_); }
  bool sendsVideo() const { return backhaul::sendsVideo(role_, returnInAll_); }
  bool receivesAnything() const { return backhaul::receivesAnything(role_); }
  QString videoDeviceName() const { return videoDeviceName_; }
  void setVideoDeviceName(const QString& name);
  // "en" (the default) or "ja". The texts in the code are Japanese; main.cpp installs the
  // English dictionary while this says "en".
  QString language() const { return language_; }
  void setLanguage(const QString& language);
  bool returnAsShare() const { return returnAsShare_; }
  void setReturnAsShare(bool share);
  // "Send the return from the all-in-one PC too". Off unless the owner switched it on, so
  // that devices remembered from another role never start sending by themselves.
  bool returnInAll() const { return returnInAll_; }
  void setReturnInAll(bool on);
  bool checksUpdates() const { return checksUpdates_; }
  bool rejoins() const { return rejoins_; }
  void setRejoins(bool on);
  void setChecksUpdates(bool on);
  bool receivesVideo() const { return backhaul::receivesVideo(role_); }
  // Slides that are not 16:9: true cuts them to 16:9, false shows all with black bars.
  bool cropSlides() const;
  void setCropSlides(bool crop);
  QString standbyText() const { return standbyText_; }
  // The plug-in rows and their settings, kept as the plug-in part of the program wrote them.
  QJsonObject pluginSettings() const { return pluginSettings_; }
  void setPluginSettings(const QJsonObject& plugins);
  void setStandbyText(const QString& text);
  static constexpr int kMaxStandbyLength = 24;
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

  QString role() const { return QString::fromLatin1(backhaul::keyOf(role_)); }
  // Unknown names are ignored. A two-shot needs two person slots: one is added when there is only one.
  void setRole(const QString& key);
  backhaul::Role roleValue() const { return role_; }
  bool handlesAudio() const { return backhaul::handlesAudio(role_); }
  // Whether the current role uses this slot row.
  bool slotActive(int row) const;
  int roleRow() const;
  // Row of the slide slot, -1 when there is none.
  int shareRow() const;
  int roleOutputChoice() const;
  int twoShotLeft() const { return twoShotSwapped_ ? 1 : 0; }
  int twoShotRight() const { return twoShotSwapped_ ? 0 : 1; }
  bool twoShotFill() const { return twoShotFill_; }
  void setTwoShotFill(bool fill);
  bool twoShotSolo() const { return twoShotSolo_; }
  void setTwoShotSolo(bool solo);
  int twoShotSplit() const { return twoShotSplit_; }

  // Puts participant `participantRow` into slot `slotRow` (replacing whoever was there).
  Q_INVOKABLE void assignTo(int slotRow, int participantRow);
  // `choice` 0 means none; choice n is the n-th monitor that can be an output.
  Q_INVOKABLE void setRoleOutputChoice(int choice);
  Q_INVOKABLE void swapTwoShot();
  QStringList pickNames() const;
  int twoShotLeftPick() const { return pickOfSlot(twoShotLeft()); }
  int twoShotRightPick() const { return pickOfSlot(twoShotRight()); }
  // `pick` is an index into pickNames(); 0 empties the slot.
  Q_INVOKABLE void pickForSlot(int slotRow, int pick);

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
  void languageChanged();
  void changed();
  // Something that settingsJson() covers was changed by the operator.
  void settingsEdited();
  // Someone was put into a slot by hand.
  void assignmentsChanged();
  void selectedChanged();
  void roleChanged();

 private:
  backhaul::Role role_ = backhaul::Role::Unset;
  QString standbyText_ = QStringLiteral("SLIDE");
  QJsonObject pluginSettings_;
  QString micDeviceName_;
  int sendQuality_ = 0;
  bool sendStereo_ = false;
  QString videoDeviceName_;
  bool returnAsShare_ = false;
  QString language_ = QStringLiteral("en");
  bool returnInAll_ = false;
  bool checksUpdates_ = true;
  bool rejoins_ = true;
  QString roleScreen_;
  bool twoShotSwapped_ = false, twoShotFill_ = true, twoShotSolo_ = false;
  int twoShotSplit_ = 0;
  void refreshTwoShotSplit();
  int pickOfSlot(int slotRow) const;
  QVector<int> pickRows() const;  // participant rows behind pickNames(), without entry 0

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
  QString joinName_ = T("会場（backhaul）");
  bool qualityAuto_ = true;
  void edited(int row);
};
