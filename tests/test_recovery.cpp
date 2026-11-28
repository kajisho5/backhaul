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
}
