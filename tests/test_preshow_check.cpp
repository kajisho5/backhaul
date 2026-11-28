#include <algorithm>

#include "check.h"
#include "core/preshow_check.h"

using namespace backhaul;

namespace {

// A PC that does everything and is ready for the show.
ShowFacts ready() {
  ShowFacts f;
  f.playsAudio = f.sendsAudio = f.showsVideo = true;
  f.netAsked = f.netReachable = true;
  f.netMs = 41;
  f.licenceActive = true;
  f.licenceDaysLeft = 300;
  f.zoomSignedIn = true;
  f.meetingIdGood = true;
  f.outputChosen = f.outputRunning = true;
  f.spareChosen = f.spareThere = true;
  f.inputChosen = f.inputRunning = true;
  f.monitorsChosen = 2;
  f.power = 1;
  f.notifications = 0;
  f.update = 1;
  return f;
}

const CheckItem* find(const std::vector<CheckItem>& items, CheckId id) {
  const auto it = std::find_if(items.begin(), items.end(), [id](const CheckItem& i) { return i.id == id; });
  return it == items.end() ? nullptr : &*it;
}

bool is(const std::vector<CheckItem>& items, CheckId id, CheckLevel level, CheckReason reason) {
  const CheckItem* item = find(items, id);
  return item != nullptr && item->level == level && item->reason == reason;
}

}  // namespace

void testPreshowCheck() {
  using enum CheckLevel;
  using enum CheckReason;

  {  // everything fine: eleven points, all OK, in the order of the screen
    const auto items = checkBeforeShow(ready());
    CHECK(items.size() == 11);
    CHECK(std::all_of(items.begin(), items.end(), [](const CheckItem& i) { return i.level == Ok; }));
    CHECK(items.front().id == CheckId::Internet && items.back().id == CheckId::Update);
    CHECK(items[1].id == CheckId::Licence && items[2].id == CheckId::ZoomSignIn && items[3].id == CheckId::MeetingId);
  }
  {  // the line
    ShowFacts f = ready();
    f.netMs = 300;
    CHECK(is(checkBeforeShow(f), CheckId::Internet, Warn, Slow));
    f.netReachable = false;
    CHECK(is(checkBeforeShow(f), CheckId::Internet, Bad, Unreachable));
    f.netAsked = false;  // not asked yet: nothing is claimed
    CHECK(find(checkBeforeShow(f), CheckId::Internet) == nullptr);
  }
  {  // the licence
    ShowFacts f = ready();
    f.licenceDaysLeft = 7;
    CHECK(is(checkBeforeShow(f), CheckId::Licence, Warn, LicenceEndsSoon));
    f.licenceDaysLeft = 8;
    CHECK(is(checkBeforeShow(f), CheckId::Licence, Ok, LicenceGood));
    f.licenceDaysLeft = -1;  // no end
    CHECK(is(checkBeforeShow(f), CheckId::Licence, Ok, LicenceGood));
    f.licenceStopped = true;
    CHECK(is(checkBeforeShow(f), CheckId::Licence, Bad, LicenceStopped));
    f.licenceActive = false;
    CHECK(is(checkBeforeShow(f), CheckId::Licence, Bad, NoLicence));
    CHECK(find(checkBeforeShow(f), CheckId::ZoomSignIn) == nullptr);  // nothing to sign in with
    f.ownKeys = true;  // Zoom keys typed into this PC stand in for the licence
    CHECK(is(checkBeforeShow(f), CheckId::Licence, Ok, OwnKeys));
    CHECK(find(checkBeforeShow(f), CheckId::ZoomSignIn) == nullptr);
  }
  {  // the sign-in is only needed for other accounts' meetings: worth a look, not a stop
    ShowFacts f = ready();
    f.zoomSignedIn = false;
    CHECK(is(checkBeforeShow(f), CheckId::ZoomSignIn, Warn, NotSignedIn));
  }
  {  // no meeting ID: nothing to join
    ShowFacts f = ready();
    f.meetingIdGood = false;
    CHECK(is(checkBeforeShow(f), CheckId::MeetingId, Bad, IdMissing));
  }
  {  // sound
    ShowFacts f = ready();
    f.outputRunning = false;
    CHECK(is(checkBeforeShow(f), CheckId::AudioOutput, Bad, DeviceNotRunning));
    f.outputChosen = false;
    CHECK(is(checkBeforeShow(f), CheckId::AudioOutput, Bad, DeviceNotChosen));
    f.inputRunning = false;
    CHECK(is(checkBeforeShow(f), CheckId::AudioInput, Bad, DeviceNotRunning));
    f.inputChosen = false;  // sending a picture without sound is allowed
    CHECK(is(checkBeforeShow(f), CheckId::AudioInput, Warn, DeviceNotChosen));
  }
  {  // the spare sound device: never a stop, but said
    ShowFacts f = ready();
    f.spareThere = false;
    CHECK(is(checkBeforeShow(f), CheckId::SpareOutput, Warn, SpareNotThere));
    f.spareChosen = false;
    CHECK(is(checkBeforeShow(f), CheckId::SpareOutput, Warn, NoSpare));
    f.spareChosen = f.spareThere = f.onSpare = true;  // the main device is gone right now
    CHECK(is(checkBeforeShow(f), CheckId::SpareOutput, Warn, SpareInUse));
    f.outputChosen = false;  // no main device at all: the point about the spare says nothing more
    CHECK(find(checkBeforeShow(f), CheckId::SpareOutput) == nullptr);
  }
  {  // monitors
    ShowFacts f = ready();
    f.monitorsMissing = 1;
    CHECK(is(checkBeforeShow(f), CheckId::VideoOutputs, Bad, OutputMissing));
    f.monitorsMissing = 0;
    f.monitorsChosen = 0;
    CHECK(is(checkBeforeShow(f), CheckId::VideoOutputs, Warn, NoOutputChosen));
  }
  {  // power, notifications, updates: said when known, left out when not
    ShowFacts f = ready();
    f.power = 0;
    f.notifications = 1;
    f.update = 2;
    const auto items = checkBeforeShow(f);
    CHECK(is(items, CheckId::Power, Warn, OnBattery));
    CHECK(is(items, CheckId::Notifications, Warn, NotificationsShown));
    CHECK(is(items, CheckId::Update, Warn, NewerWaiting));
    f.power = f.notifications = -1;
    f.update = 0;
    const auto fewer = checkBeforeShow(f);
    CHECK(find(fewer, CheckId::Power) == nullptr && find(fewer, CheckId::Notifications) == nullptr && find(fewer, CheckId::Update) == nullptr);
    CHECK(fewer.size() == 8);
  }
  {  // a PC that only shows one person: no points about sound
    ShowFacts f = ready();
    f.playsAudio = f.sendsAudio = false;
    f.outputChosen = f.outputRunning = f.inputChosen = f.inputRunning = false;
    const auto items = checkBeforeShow(f);
    CHECK(find(items, CheckId::AudioOutput) == nullptr && find(items, CheckId::AudioInput) == nullptr);
    CHECK(find(items, CheckId::SpareOutput) == nullptr);
    CHECK(std::all_of(items.begin(), items.end(), [](const CheckItem& i) { return i.level == Ok; }));
  }
  {  // a PC that only handles sound: no point about monitors
    ShowFacts f = ready();
    f.showsVideo = false;
    f.monitorsChosen = 0;
    CHECK(find(checkBeforeShow(f), CheckId::VideoOutputs) == nullptr);
  }
}
