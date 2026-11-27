// What one PC is used for. In a venue each PC usually does one job: one shows a
// single presenter, another shows two people side by side, and so on.
#pragma once

#include <string_view>

namespace backhaul {

enum class Role {
  Unset,    // nothing chosen yet: the program asks
  All,      // every slot, picture and sound, on this one PC
  Camera,   // one person, full screen
  TwoShot,  // two people side by side in one picture
};

// How many person slots the role uses, counted from the first; 0 means all of them.
int personSlotsOf(Role role);

// Whether this PC takes the meeting's sound. Only one PC per meeting should.
bool handlesAudio(Role role);

// Name used in the settings file ("" for Unset).
const char* keyOf(Role role);
// False when `key` is not a role name; `out` is then left alone.
bool roleFromKey(std::string_view key, Role* out);

// Two-shot picture: which halves are drawn.
enum class TwoShotSplit { Both, LeftOnly, RightOnly };

// `soloWhenMissing`: when one side has no picture, the other side takes the whole
// screen. Otherwise both halves stay (the side without a picture shows its name card).
TwoShotSplit twoShotSplit(bool leftHasPicture, bool rightHasPicture, bool soloWhenMissing);

}  // namespace backhaul
