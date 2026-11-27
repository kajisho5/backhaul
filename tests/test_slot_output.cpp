#include "check.h"
#include "core/slot_output.h"

using namespace backhaul;

namespace {
SlotSituation live() {
  SlotSituation s;
  s.assigned = s.connected = s.videoOn = s.audioOn = s.hasLastFrame = true;
  return s;
}
}  // namespace

void testSlotOutput() {
  {  // nothing assigned: black and silent
    SlotDecision d = decide(SlotSituation{});
    CHECK(d.video == VideoOut::Black && !d.audio && d.ui == UiState::Unassigned);
  }
  {  // normal
    SlotDecision d = decide(live());
    CHECK(d.video == VideoOut::Live && d.audio && d.ui == UiState::Ok && !d.uiMuted);
  }
  {  // camera off: name card, audio continues
    SlotSituation s = live();
    s.videoOn = false;
    SlotDecision d = decide(s);
    CHECK(d.video == VideoOut::NameCard && d.audio && d.ui == UiState::CameraOff && !d.uiMuted);
  }
  {  // muted: picture continues, silent
    SlotSituation s = live();
    s.audioOn = false;
    SlotDecision d = decide(s);
    CHECK(d.video == VideoOut::Live && !d.audio && d.ui == UiState::Muted && !d.uiMuted);
  }
  {  // camera off and muted at once: name card, silent, both shown
    SlotSituation s = live();
    s.videoOn = s.audioOn = false;
    SlotDecision d = decide(s);
    CHECK(d.video == VideoOut::NameCard && !d.audio && d.ui == UiState::CameraOff && d.uiMuted);
  }
  {  // unstable network: output is untouched, only the operator screen changes
    SlotSituation s = live();
    s.unstable = true;
    SlotDecision d = decide(s);
    CHECK(d.video == VideoOut::Live && d.audio && d.ui == UiState::Unstable);
  }
  {  // lost, policy "freeze"
    SlotSituation s = live();
    s.connected = false;
    SlotDecision d = decide(s);
    CHECK(d.video == VideoOut::Freeze && !d.audio && d.ui == UiState::Lost);
  }
  {  // lost, policy "freeze" but no picture was ever received: fall back to the name card
    SlotSituation s = live();
    s.connected = false;
    s.hasLastFrame = false;
    CHECK(decide(s).video == VideoOut::NameCard);
  }
  {  // lost, policy "name card"
    SlotSituation s = live();
    s.connected = false;
    s.onDisconnect = DisconnectPolicy::NameCard;
    SlotDecision d = decide(s);
    CHECK(d.video == VideoOut::NameCard && !d.audio && d.ui == UiState::Lost);
  }
  {  // lost participant's stale flags must not leak audio
    SlotSituation s = live();
    s.connected = false;
    s.audioOn = true;
    CHECK(!decide(s).audio);
  }
  {  // slide slot with nobody sharing: black, and not an alarm
    SlotSituation s = live();
    s.kind = SlotKind::Share;
    s.videoOn = false;
    s.audioOn = false;
    SlotDecision d = decide(s);
    CHECK(d.video == VideoOut::Black && d.ui == UiState::Ok && !d.uiMuted);
  }
  {  // slide slot, sharer dropped, "name card" policy: a share has no card, so black
    SlotSituation s = live();
    s.kind = SlotKind::Share;
    s.connected = false;
    s.onDisconnect = DisconnectPolicy::NameCard;
    CHECK(decide(s).video == VideoOut::Black);
  }
}
