// Decides what a slot sends to its HDMI output and audio channel, and which
// state the operator screen shows, from the slot's current situation.
#pragma once

namespace backhaul {

enum class SlotKind { Person, Share };
enum class DisconnectPolicy { Freeze, NameCard };

struct SlotSituation {
  SlotKind kind = SlotKind::Person;
  bool assigned = false;      // a participant is assigned to this slot
  bool connected = false;     // that participant is in the meeting right now
  bool videoOn = false;       // camera on (person) / sharing (share)
  bool audioOn = false;       // not muted
  bool unstable = false;      // network trouble reported for this participant
  bool hasLastFrame = false;  // at least one picture has been received before
  DisconnectPolicy onDisconnect = DisconnectPolicy::Freeze;
};

enum class VideoOut { Black, Live, NameCard, Freeze };
enum class UiState { Unassigned, Ok, CameraOff, Muted, Unstable, Lost };

struct SlotDecision {
  VideoOut video = VideoOut::Black;
  bool audio = false;
  UiState ui = UiState::Unassigned;  // most urgent state, shown as colour + words
  bool uiMuted = false;              // shown in addition when ui is not Muted
};

SlotDecision decide(const SlotSituation& s);

}  // namespace backhaul
