#include "check.h"
#include "core/role.h"

using namespace backhaul;

void testRole() {
  // Slots each role uses.
  CHECK(personSlotsOf(Role::Camera) == 1);
  CHECK(personSlotsOf(Role::TwoShot) == 2);
  CHECK(personSlotsOf(Role::All) == 0);
  CHECK(personSlotsOf(Role::Unset) == 0);

  // Only the "everything" role takes the meeting's sound.
  CHECK(handlesAudio(Role::All));
  CHECK(!handlesAudio(Role::Camera));
  CHECK(!handlesAudio(Role::TwoShot));
  CHECK(!handlesAudio(Role::Unset));

  // Settings names go both ways; unknown names are refused and leave the value alone.
  for (const Role r : {Role::Unset, Role::All, Role::Camera, Role::TwoShot}) {
    Role back = r == Role::All ? Role::Camera : Role::All;
    CHECK(roleFromKey(keyOf(r), &back));
    CHECK(back == r);
  }
  Role kept = Role::TwoShot;
  CHECK(!roleFromKey("Camera", &kept));
  CHECK(!roleFromKey("slide", &kept));
  CHECK(kept == Role::TwoShot);

  // Two-shot halves.
  CHECK(twoShotSplit(true, true, false) == TwoShotSplit::Both);
  CHECK(twoShotSplit(true, true, true) == TwoShotSplit::Both);
  CHECK(twoShotSplit(true, false, false) == TwoShotSplit::Both);   // name card stays in its half
  CHECK(twoShotSplit(true, false, true) == TwoShotSplit::LeftOnly);
  CHECK(twoShotSplit(false, true, true) == TwoShotSplit::RightOnly);
  CHECK(twoShotSplit(false, false, true) == TwoShotSplit::Both);   // nobody to enlarge
}
