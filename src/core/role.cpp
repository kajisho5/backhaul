#include "core/role.h"

namespace backhaul {

int personSlotsOf(Role role) {
  switch (role) {
    case Role::Camera: return 1;
    case Role::TwoShot: return 2;
    case Role::Unset:
    case Role::All:
    case Role::Slide:
    case Role::Audio: break;
  }
  return 0;
}

bool usesPeople(Role role) { return role != Role::Slide; }

bool usesShare(Role role) { return role == Role::All || role == Role::Slide; }

bool singleOutput(Role role) { return role == Role::Camera || role == Role::TwoShot || role == Role::Slide; }

bool handlesAudio(Role role) { return role == Role::All || role == Role::Audio; }

bool receivesVideo(Role role) { return role != Role::Audio && role != Role::Unset; }

bool sendsAudio(Role role) { return role == Role::Audio; }

const char* keyOf(Role role) {
  switch (role) {
    case Role::All: return "all";
    case Role::Camera: return "camera";
    case Role::TwoShot: return "twoshot";
    case Role::Slide: return "slide";
    case Role::Audio: return "audio";
    case Role::Unset: break;
  }
  return "";
}

bool roleFromKey(std::string_view key, Role* out) {
  for (const Role r : {Role::Unset, Role::All, Role::Camera, Role::TwoShot, Role::Slide, Role::Audio}) {
    if (key == keyOf(r)) {
      *out = r;
      return true;
    }
  }
  return false;
}

TwoShotSplit twoShotSplit(bool leftHasPicture, bool rightHasPicture, bool soloWhenMissing) {
  if (!soloWhenMissing || leftHasPicture == rightHasPicture) return TwoShotSplit::Both;
  return leftHasPicture ? TwoShotSplit::LeftOnly : TwoShotSplit::RightOnly;
}

}  // namespace backhaul
