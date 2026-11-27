#include "core/quality_policy.h"

namespace backhaul {

int heightOf(Resolution r) {
  switch (r) {
    case Resolution::R90: return 90;
    case Resolution::R180: return 180;
    case Resolution::R360: return 360;
    case Resolution::R720: return 720;
    case Resolution::R1080: return 1080;
  }
  return 0;
}

int maxSimultaneous(Resolution r) {
  switch (r) {
    case Resolution::R1080: return 1;
    case Resolution::R720: return 2;
    case Resolution::R360: return 4;
    case Resolution::R180: return 17;
    case Resolution::R90: return 34;
  }
  return 0;
}

Resolution pickPersonResolution(int personCount, QualityCap cap) {
  static constexpr Resolution kBestFirst[] = {Resolution::R1080, Resolution::R720, Resolution::R360,
                                              Resolution::R180, Resolution::R90};
  for (Resolution r : kBestFirst) {
    if (cap == QualityCap::Max720 && r == Resolution::R1080) continue;
    if (personCount <= maxSimultaneous(r)) return r;
  }
  return Resolution::R90;
}

}  // namespace backhaul
