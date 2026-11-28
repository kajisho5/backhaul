// The check before the show: from what is known about this PC right now, which points
// are fine, which deserve a look, and which will stop the show. The words for the screen
// are made elsewhere; nothing here knows Qt, Windows or the meeting service.
#pragma once

#include <vector>

namespace backhaul {

enum class CheckLevel { Ok, Warn, Bad };  // Warn: the show can go on; Bad: it will not work like this

enum class CheckId { Internet, Licence, ZoomSignIn, MeetingId, AudioOutput, SpareOutput, AudioInput, VideoOutputs, Power, Notifications, Update };

enum class CheckReason {
  // Internet
  Reachable, Slow, Unreachable,
  // Licence
  LicenceGood, LicenceEndsSoon, LicenceStopped, NoLicence, OwnKeys,
  // Zoom sign-in
  SignedIn, NotSignedIn,
  // Meeting ID
  IdGood, IdMissing,
  // Devices (sound out, sound in)
  DeviceRunning, DeviceNotRunning, DeviceNotChosen,
  // The spare sound device
  SpareReady, SpareInUse, SpareNotThere, NoSpare,
  // Monitors
  OutputsGood, OutputMissing, NoOutputChosen,
  // Power
  OnMains, OnBattery,
  // Windows notifications
  NotificationsHeld, NotificationsShown,
  // Updates
  UpToDate, NewerWaiting,
};

// What is known. "Unknown" values leave their point out instead of guessing.
struct ShowFacts {
  // What this PC's role does.
  bool playsAudio = false, sendsAudio = false, showsVideo = false;
  // The licence server: whether it was asked, whether it answered, and how fast.
  bool netAsked = false, netReachable = false;
  int netMs = 0;
  // Zoom keys typed into this PC are used in place of a licence.
  bool ownKeys = false;
  bool licenceActive = false, licenceStopped = false;
  long long licenceDaysLeft = -1;  // -1: no end
  bool zoomSignedIn = false;
  bool meetingIdGood = false;
  bool outputChosen = false, outputRunning = false;
  bool spareChosen = false, spareThere = false, onSpare = false;
  bool inputChosen = false, inputRunning = false;
  int monitorsChosen = 0, monitorsMissing = 0;
  int power = -1;          // -1 unknown, 0 battery, 1 mains
  int notifications = -1;  // -1 unknown, 0 held back, 1 shown
  int update = 0;          // 0 unknown, 1 up to date, 2 a newer version is waiting
};

struct CheckItem {
  CheckId id;
  CheckLevel level;
  CheckReason reason;
  bool operator==(const CheckItem&) const = default;
};

inline constexpr int kSlowLineMs = 300;       // an answer slower than this is worth a look
inline constexpr int kLicenceSoonDays = 7;    // a licence ending within this many days is worth a look

// In the order they are shown. Points that do not concern this PC's role are left out.
std::vector<CheckItem> checkBeforeShow(const ShowFacts& facts);

}  // namespace backhaul
