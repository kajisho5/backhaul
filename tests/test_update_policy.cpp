#include "check.h"
#include "core/update_policy.h"

using namespace backhaul;

void testUpdatePolicy() {
  // Version numbers as they are written.
  CHECK((parseVersion("0.1.0") == Version{0, 1, 0}));
  CHECK((parseVersion("v12.34.567") == Version{12, 34, 567}));
  for (const char* bad : {"", "v", "1", "1.2", "1.2.", "1.2.3.4", "1.2.3-beta", " 1.2.3", "1.2.3 ", "1..3", "a.b.c", "1.2.x", "1234567.0.0", "-1.0.0"}) {
    CHECK(!parseVersion(bad).has_value());
  }

  // Later means later in the first part that differs.
  CHECK((isNewer({0, 2, 0}, {0, 1, 9})));
  CHECK((isNewer({1, 0, 0}, {0, 99, 99})));
  CHECK((isNewer({0, 1, 10}, {0, 1, 9})));  // numbers, not text: 10 comes after 9
  CHECK((!isNewer({0, 1, 0}, {0, 1, 0})));
  CHECK((!isNewer({0, 1, 0}, {0, 2, 0})));
  CHECK((!isNewer({0, 9, 9}, {1, 0, 0})));

  // The notice: only for a newer version, never in a meeting, not after "later".
  CHECK(showsUpdateNotice("0.1.0", "0.2.0", false, false));
  CHECK(!showsUpdateNotice("0.1.0", "0.2.0", true, false));
  CHECK(!showsUpdateNotice("0.1.0", "0.2.0", false, true));
  CHECK(!showsUpdateNotice("0.2.0", "0.2.0", false, false));
  CHECK(!showsUpdateNotice("0.3.0", "0.2.0", false, false));  // an older one on the server is not offered
  CHECK(!showsUpdateNotice("0.1.0", "", false, false));
  CHECK(!showsUpdateNotice("0.1.0", "latest", false, false));
  CHECK(!showsUpdateNotice("dev", "0.2.0", false, false));
}
