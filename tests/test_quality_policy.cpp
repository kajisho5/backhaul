#include "check.h"
#include "core/quality_policy.h"

using namespace backhaul;

void testQualityPolicy() {
  // One pinned presenter gets the best the connection allows.
  CHECK(pickPersonResolution(1, QualityCap::Auto) == Resolution::R1080);
  CHECK(pickPersonResolution(2, QualityCap::Auto) == Resolution::R720);
  CHECK(pickPersonResolution(3, QualityCap::Auto) == Resolution::R360);
  CHECK(pickPersonResolution(4, QualityCap::Auto) == Resolution::R360);
  CHECK(pickPersonResolution(5, QualityCap::Auto) == Resolution::R180);
  CHECK(pickPersonResolution(17, QualityCap::Auto) == Resolution::R180);
  CHECK(pickPersonResolution(18, QualityCap::Auto) == Resolution::R90);
  CHECK(pickPersonResolution(34, QualityCap::Auto) == Resolution::R90);
  CHECK(pickPersonResolution(100, QualityCap::Auto) == Resolution::R90);

  // The "up to 720p" setting only changes the single-person case.
  CHECK(pickPersonResolution(1, QualityCap::Max720) == Resolution::R720);
  CHECK(pickPersonResolution(2, QualityCap::Max720) == Resolution::R720);
  CHECK(pickPersonResolution(3, QualityCap::Max720) == Resolution::R360);

  // Nobody assigned yet must not crash or ask for less than the best.
  CHECK(pickPersonResolution(0, QualityCap::Auto) == Resolution::R1080);

  CHECK(heightOf(Resolution::R1080) == 1080);
  CHECK(heightOf(Resolution::R90) == 90);
  CHECK(maxSimultaneous(Resolution::R1080) == 1);
  CHECK(maxSimultaneous(Resolution::R720) == 2);
  CHECK(maxSimultaneous(Resolution::R360) == 4);
}
