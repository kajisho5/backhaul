#include "core/role.h"

namespace backhaul {

int personSlotsOf(Role role) {
  switch (role) {
    case Role::Camera: return 1;
    case Role::TwoShot: return 2;
    case Role::Unset:
    case Role::All:
    case Role::Slide:
    case Role::Audio:
    case Role::Return: break;
  }
  return 0;
}

bool usesPeople(Role role) { return role != Role::Slide && role != Role::Return; }

bool usesShare(Role role) { return role == Role::All || role == Role::Slide; }

bool singleOutput(Role role) { return role == Role::Camera || role == Role::TwoShot || role == Role::Slide; }

bool playsAudio(Role role) { return role == Role::All || role == Role::Audio; }

bool handlesAudio(Role role) { return playsAudio(role) || sendsAudio(role); }  // the all-in-one PC handles sound either way

bool receivesVideo(Role role) { return role != Role::Audio && role != Role::Return && role != Role::Unset; }

bool sendsAudio(Role role) { return role == Role::Audio || role == Role::Return; }

bool sendsVideo(Role role) { return role == Role::Return; }

bool sendsAudio(Role role, bool returnInAll) { return sendsAudio(role) || (role == Role::All && returnInAll); }

bool sendsVideo(Role role, bool returnInAll) { return sendsVideo(role) || (role == Role::All && returnInAll); }

bool receivesAnything(Role role) { return receivesVideo(role) || playsAudio(role); }

const char* keyOf(Role role) {
  switch (role) {
    case Role::All: return "all";
    case Role::Camera: return "camera";
    case Role::TwoShot: return "twoshot";
    case Role::Slide: return "slide";
    case Role::Audio: return "audio";
    case Role::Return: return "return";
    case Role::Unset: break;
  }
  return "";
}

bool roleFromKey(std::string_view key, Role* out) {
  for (const Role r : {Role::Unset, Role::All, Role::Camera, Role::TwoShot, Role::Slide, Role::Audio, Role::Return}) {
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
