#include "core/slot_output.h"

namespace backhaul {

SlotDecision decide(const SlotSituation& s) {
  SlotDecision d;
  if (!s.assigned) return d;  // black, silent, Unassigned

  if (!s.connected) {
    d.ui = UiState::Lost;
    d.audio = false;
    if (s.kind == SlotKind::Share) {
      // A share has no name card: hold the last slide if asked to, otherwise black.
      d.video = (s.onDisconnect == DisconnectPolicy::Freeze && s.hasLastFrame) ? VideoOut::Freeze
                                                                               : VideoOut::Black;
    } else {
      d.video = (s.onDisconnect == DisconnectPolicy::Freeze && s.hasLastFrame) ? VideoOut::Freeze
                                                                               : VideoOut::NameCard;
    }
    return d;
  }

  d.audio = s.audioOn;
  const bool muted = !s.audioOn;

  if (!s.videoOn) {
    d.video = (s.kind == SlotKind::Share) ? VideoOut::Black : VideoOut::NameCard;
  } else {
    d.video = VideoOut::Live;
  }

  // Most urgent first. A share that is simply not running is not an alarm.
  if (s.unstable) {
    d.ui = UiState::Unstable;
  } else if (!s.videoOn && s.kind == SlotKind::Person) {
    d.ui = UiState::CameraOff;
  } else if (muted && s.kind == SlotKind::Person) {
    d.ui = UiState::Muted;
  } else {
    d.ui = UiState::Ok;
  }
  d.uiMuted = muted && s.kind == SlotKind::Person && d.ui != UiState::Muted;
  return d;
}

}  // namespace backhaul
