// Coming back on its own: when this PC joins a meeting again after it was thrown out,
// and how long it waits between tries. Devices that went away are looked for at a
// steady pace. Nothing here knows the meeting service, Qt or Windows.
#pragma once

#include <cstdint>
#include <vector>

namespace backhaul {

// Why a meeting is over for this PC, as far as the meeting service said.
enum class EndCause {
  None,        // nothing was said (also: this PC left by itself)
  Connection,  // the line: the servers could not be reached, or a reconnect failed
  Host,        // the host ended the meeting or took this PC out; the meeting ran out of time
  Refused,     // the meeting does not take this PC: wrong passcode, no such meeting, full, locked
  Other,       // anything else
};

// Only a lost line is worth trying again: everything else would fail the same way,
// or go against what the host decided.
bool shouldRejoin(EndCause cause);

// Seconds to wait before try number `attempt` (1 = the first try after the drop):
// 5, 10, 20, then 30 every time. Less than 1 counts as 1.
int rejoinWaitSeconds(int attempt);

// After this long without getting back in, trying stops.
inline constexpr int kRejoinGiveUpSeconds = 600;

// How often a device that went away is looked for.
inline constexpr int kDeviceSeekSeconds = 2;

// The sound output has a main device and, when the operator chose one, a spare. With no
// output running: the main device whenever it is there; the spare only after the main
// one has been gone this long (a quick re-plug is not worth a switch).
inline constexpr std::int64_t kSpareAfterMs = 3000;
enum class OutputPick { None, Main, Spare };
OutputPick pickOutput(bool mainThere, bool spareThere, std::int64_t mainGoneMs);

// A program that crashed is started again, but not for ever: the crash that makes it
// this many within the window is left alone (two are answered, the third is not).
inline constexpr int kCrashLimit = 3;
inline constexpr std::int64_t kRestartWindowMs = 5 * 60 * 1000;
// `earlier` are the times of the crashes answered with a restart so far (any order).
// True when the crash that has just happened may be answered with one more.
bool mayRestart(const std::vector<std::int64_t>& earlier, std::int64_t nowMs);

}  // namespace backhaul
