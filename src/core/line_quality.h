// How good the line is, in three grades the operator can act on, and when a person's
// line counts as unstable. Nothing here knows the meeting service, Qt or Windows.
#pragma once

#include <cstdint>
#include <map>
#include <vector>

namespace backhaul {

enum class LineGrade { Unknown, Good, Fair, Poor };

// The meeting service's own scale: 0 unknown, 1 very bad, 2 bad, 3 not good, 4 normal,
// 5 good, 6 excellent. Anything outside is unknown.
LineGrade gradeOfStep(int step);

// The worse of the two; an unknown one does not count.
LineGrade worse(LineGrade a, LineGrade b);

// A person whose line has been poor for this long is shown as unstable; a short dip is not.
inline constexpr std::int64_t kUnstableAfterMs = 2000;

// Remembers since when each person's line is poor.
class UnstableWatch {
 public:
  // What the meeting service says about one person right now.
  void report(std::uint32_t user, bool poor, std::int64_t nowMs);
  bool unstable(std::uint32_t user, std::int64_t nowMs) const;
  void forget(std::uint32_t user) { poorSince_.erase(user); }
  // Forgets everybody who is not in `users`.
  void keepOnly(const std::vector<std::uint32_t>& users);
  void clear() { poorSince_.clear(); }

 private:
  std::map<std::uint32_t, std::int64_t> poorSince_;
};

}  // namespace backhaul
