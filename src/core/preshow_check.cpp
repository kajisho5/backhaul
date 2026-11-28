#include "core/preshow_check.h"

namespace backhaul {

std::vector<CheckItem> checkBeforeShow(const ShowFacts& f) {
  using enum CheckLevel;
  using enum CheckReason;
  std::vector<CheckItem> out;

  if (f.netAsked) {
    if (!f.netReachable) {
      out.push_back({CheckId::Internet, Bad, Unreachable});
    } else if (f.netMs >= kSlowLineMs) {
      out.push_back({CheckId::Internet, Warn, Slow});
    } else {
      out.push_back({CheckId::Internet, Ok, Reachable});
    }
  }

  if (f.ownKeys) {
    out.push_back({CheckId::Licence, Ok, OwnKeys});
  } else if (!f.licenceActive) {
    out.push_back({CheckId::Licence, Bad, NoLicence});
  } else if (f.licenceStopped) {
    out.push_back({CheckId::Licence, Bad, LicenceStopped});
  } else if (f.licenceDaysLeft >= 0 && f.licenceDaysLeft <= kLicenceSoonDays) {
    out.push_back({CheckId::Licence, Warn, LicenceEndsSoon});
  } else {
    out.push_back({CheckId::Licence, Ok, LicenceGood});
  }

  // With own keys there is no shared sign-in: the permission is typed by hand when needed.
  if (!f.ownKeys && f.licenceActive) {
    out.push_back(f.zoomSignedIn ? CheckItem{CheckId::ZoomSignIn, Ok, SignedIn} : CheckItem{CheckId::ZoomSignIn, Warn, NotSignedIn});
  }

  out.push_back(f.meetingIdGood ? CheckItem{CheckId::MeetingId, Ok, IdGood} : CheckItem{CheckId::MeetingId, Bad, IdMissing});

  if (f.playsAudio) {
    if (!f.outputChosen) {
      out.push_back({CheckId::AudioOutput, Bad, DeviceNotChosen});
    } else {
      out.push_back(f.outputRunning ? CheckItem{CheckId::AudioOutput, Ok, DeviceRunning} : CheckItem{CheckId::AudioOutput, Bad, DeviceNotRunning});
    }
  }
  if (f.sendsAudio) {
    if (!f.inputChosen) {
      out.push_back({CheckId::AudioInput, Warn, DeviceNotChosen});  // a PC may send a picture only
    } else {
      out.push_back(f.inputRunning ? CheckItem{CheckId::AudioInput, Ok, DeviceRunning} : CheckItem{CheckId::AudioInput, Bad, DeviceNotRunning});
    }
  }

  if (f.showsVideo) {
    if (f.monitorsMissing > 0) {
      out.push_back({CheckId::VideoOutputs, Bad, OutputMissing});
    } else if (f.monitorsChosen == 0) {
      out.push_back({CheckId::VideoOutputs, Warn, NoOutputChosen});
    } else {
      out.push_back({CheckId::VideoOutputs, Ok, OutputsGood});
    }
  }

  if (f.power == 1) out.push_back({CheckId::Power, Ok, OnMains});
  if (f.power == 0) out.push_back({CheckId::Power, Warn, OnBattery});

  if (f.notifications == 0) out.push_back({CheckId::Notifications, Ok, NotificationsHeld});
  if (f.notifications == 1) out.push_back({CheckId::Notifications, Warn, NotificationsShown});

  if (f.update == 1) out.push_back({CheckId::Update, Ok, UpToDate});
  if (f.update == 2) out.push_back({CheckId::Update, Warn, NewerWaiting});

  return out;
}

}  // namespace backhaul
