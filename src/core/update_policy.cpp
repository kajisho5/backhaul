#include "core/update_policy.h"

#include <array>

namespace backhaul {

std::optional<Version> parseVersion(std::string_view text) {
  if (!text.empty() && text.front() == 'v') text.remove_prefix(1);
  std::array<int, 3> parts{};
  std::size_t at = 0;
  for (std::size_t part = 0; part < parts.size(); ++part) {
    if (part > 0) {
      if (at >= text.size() || text[at] != '.') return std::nullopt;
      ++at;
    }
    const std::size_t start = at;
    int value = 0;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
      if (at - start >= 6) return std::nullopt;  // no real version number is this long
      value = value * 10 + (text[at] - '0');
      ++at;
    }
    if (at == start) return std::nullopt;
    parts[part] = value;
  }
  if (at != text.size()) return std::nullopt;
  return Version{parts[0], parts[1], parts[2]};
}

bool isNewer(const Version& candidate, const Version& current) {
  if (candidate.major != current.major) return candidate.major > current.major;
  if (candidate.minor != current.minor) return candidate.minor > current.minor;
  return candidate.patch > current.patch;
}

bool showsUpdateNotice(std::string_view current, std::string_view latest, bool inMeeting, bool saidLater) {
  if (inMeeting || saidLater) return false;
  const auto have = parseVersion(current);
  const auto offered = parseVersion(latest);
  return have.has_value() && offered.has_value() && isNewer(*offered, *have);
}

}  // namespace backhaul
