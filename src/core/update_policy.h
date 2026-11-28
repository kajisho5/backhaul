// Updates: comparing version numbers, and when the operator may be told about a new one.
// The rule of the house: nothing about updates happens while this PC is in a meeting.
#pragma once

#include <optional>
#include <string_view>

namespace backhaul {

struct Version {
  int major = 0;
  int minor = 0;
  int patch = 0;
  friend bool operator==(const Version&, const Version&) = default;
};

/// "1.2.3" (a leading "v" is allowed). Nothing else: no "1.2", no "1.2.3-beta", no spaces.
std::optional<Version> parseVersion(std::string_view text);

/// True when `candidate` is a later version than `current`.
bool isNewer(const Version& candidate, const Version& current);

/// Whether the notice "a new version is available" is shown: only for a version that
/// really is newer, not in a meeting, and not after the operator said "later" in this run.
/// Text that is not a version number never shows a notice.
bool showsUpdateNotice(std::string_view current, std::string_view latest, bool inMeeting, bool saidLater);

}  // namespace backhaul
