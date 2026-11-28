#include "check.h"
#include "core/recovery.h"

using namespace backhaul;

void testRecovery() {
  // Only a lost line is tried again.
  CHECK(shouldRejoin(EndCause::Connection));
  CHECK(!shouldRejoin(EndCause::None));
  CHECK(!shouldRejoin(EndCause::Host));     // the host's decision stands
  CHECK(!shouldRejoin(EndCause::Refused));  // a wrong passcode stays wrong
  CHECK(!shouldRejoin(EndCause::Other));

  // Quick at first, then every half minute.
  CHECK(rejoinWaitSeconds(1) == 5);
  CHECK(rejoinWaitSeconds(2) == 10);
  CHECK(rejoinWaitSeconds(3) == 20);
  CHECK(rejoinWaitSeconds(4) == 30);
  CHECK(rejoinWaitSeconds(50) == 30);
  CHECK(rejoinWaitSeconds(0) == 5 && rejoinWaitSeconds(-3) == 5);

  // Ten minutes of trying hold at least twenty tries.
  int spent = 0, tries = 0;
  while (spent + rejoinWaitSeconds(tries + 1) <= kRejoinGiveUpSeconds) spent += rejoinWaitSeconds(++tries);
  CHECK(tries >= 20);
  CHECK(kDeviceSeekSeconds == 2);

  // The main device whenever it is there; the spare only after three seconds without it.
  CHECK(pickOutput(true, true, 0) == OutputPick::Main);
  CHECK(pickOutput(true, false, 99999) == OutputPick::Main);
  CHECK(pickOutput(false, true, 2999) == OutputPick::None);
  CHECK(pickOutput(false, true, 3000) == OutputPick::Spare);
  CHECK(pickOutput(false, false, 99999) == OutputPick::None);

  // Started again after a crash, but not more than three times in five minutes.
  const std::int64_t minute = 60 * 1000;
  CHECK(mayRestart({}, 10 * minute));
  CHECK(mayRestart({9 * minute, 8 * minute}, 10 * minute));
  CHECK(!mayRestart({9 * minute, 8 * minute, 7 * minute}, 10 * minute));
  CHECK(mayRestart({9 * minute, 8 * minute, 5 * minute}, 10 * minute));      // five minutes ago no longer counts
  CHECK(mayRestart({9 * minute, 8 * minute, 4 * minute, 1 * minute}, 10 * minute));
  CHECK(mayRestart({11 * minute, 12 * minute, 13 * minute}, 10 * minute));   // a clock set back: times "from the future" do not count
}
