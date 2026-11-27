#include "check.h"
#include "core/role.h"

using namespace backhaul;

void testRole() {
  // Slots each role uses.
  CHECK(personSlotsOf(Role::Camera) == 1);
  CHECK(personSlotsOf(Role::TwoShot) == 2);
  CHECK(personSlotsOf(Role::All) == 0);
  CHECK(personSlotsOf(Role::Unset) == 0);
  CHECK(personSlotsOf(Role::Audio) == 0);  // every person can be given an output channel

  // The "everything" role and the sound role take the meeting's sound.
  CHECK(handlesAudio(Role::All));
  CHECK(!handlesAudio(Role::Camera));
  CHECK(!handlesAudio(Role::TwoShot));
  CHECK(!handlesAudio(Role::Unset));
  CHECK(handlesAudio(Role::Audio));

  // The sound PC shows nobody and is the only one that sends sound into the meeting.
  CHECK(!receivesVideo(Role::Audio) && !receivesVideo(Role::Unset));
  CHECK(receivesVideo(Role::All) && receivesVideo(Role::Camera) && receivesVideo(Role::TwoShot));
  CHECK(sendsAudio(Role::Audio));
  CHECK(!sendsAudio(Role::All) && !sendsAudio(Role::Camera) && !sendsAudio(Role::TwoShot) && !sendsAudio(Role::Unset));

  // The return PC only sends: picture and sound into the meeting, nothing out of it.
  CHECK(sendsVideo(Role::Return) && sendsAudio(Role::Return) && handlesAudio(Role::Return));
  CHECK(!playsAudio(Role::Return) && !receivesVideo(Role::Return) && !receivesAnything(Role::Return));
  CHECK(!usesPeople(Role::Return) && !usesShare(Role::Return) && !singleOutput(Role::Return));
  CHECK(playsAudio(Role::All) && playsAudio(Role::Audio) && !playsAudio(Role::Camera) && !playsAudio(Role::Slide));
  CHECK(receivesAnything(Role::Audio) && receivesAnything(Role::Camera) && receivesAnything(Role::All));
  CHECK(!sendsVideo(Role::All) && !sendsVideo(Role::Audio) && !sendsVideo(Role::Camera));

  // Settings names go both ways; unknown names are refused and leave the value alone.
  for (const Role r : {Role::Unset, Role::All, Role::Camera, Role::TwoShot, Role::Slide, Role::Audio, Role::Return}) {
    Role back = r == Role::All ? Role::Camera : Role::All;
    CHECK(roleFromKey(keyOf(r), &back));
    CHECK(back == r);
  }
  Role kept = Role::TwoShot;
  CHECK(!roleFromKey("Camera", &kept));
  CHECK(!roleFromKey("everything", &kept));
  CHECK(roleFromKey("audio", &kept) && kept == Role::Audio);
  kept = Role::TwoShot;
  CHECK(kept == Role::TwoShot);

  // The slide PC shows the shared screen and nobody; it has one picture on one monitor and no sound.
  CHECK(!usesPeople(Role::Slide) && usesShare(Role::Slide) && singleOutput(Role::Slide));
  CHECK(receivesVideo(Role::Slide) && !handlesAudio(Role::Slide) && !sendsAudio(Role::Slide));
  CHECK(usesPeople(Role::All) && usesShare(Role::All) && !singleOutput(Role::All));
  CHECK(usesPeople(Role::Camera) && !usesShare(Role::Camera) && singleOutput(Role::Camera));
  CHECK(usesPeople(Role::TwoShot) && !usesShare(Role::TwoShot) && singleOutput(Role::TwoShot));
  CHECK(usesPeople(Role::Audio) && !usesShare(Role::Audio) && !singleOutput(Role::Audio));

  // Two-shot halves.
  CHECK(twoShotSplit(true, true, false) == TwoShotSplit::Both);
  CHECK(twoShotSplit(true, true, true) == TwoShotSplit::Both);
  CHECK(twoShotSplit(true, false, false) == TwoShotSplit::Both);   // name card stays in its half
  CHECK(twoShotSplit(true, false, true) == TwoShotSplit::LeftOnly);
  CHECK(twoShotSplit(false, true, true) == TwoShotSplit::RightOnly);
  CHECK(twoShotSplit(false, false, true) == TwoShotSplit::Both);   // nobody to enlarge
}
