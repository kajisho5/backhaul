#include "check.h"
#include "core/line_quality.h"

using namespace backhaul;

void testLineQuality() {
  // The meeting service's six steps in three grades.
  CHECK(gradeOfStep(0) == LineGrade::Unknown);
  CHECK(gradeOfStep(1) == LineGrade::Poor && gradeOfStep(2) == LineGrade::Poor);
  CHECK(gradeOfStep(3) == LineGrade::Fair);
  CHECK(gradeOfStep(4) == LineGrade::Good && gradeOfStep(5) == LineGrade::Good && gradeOfStep(6) == LineGrade::Good);
  CHECK(gradeOfStep(7) == LineGrade::Unknown && gradeOfStep(-1) == LineGrade::Unknown);

  // The worse one counts; an unknown one does not.
  CHECK(worse(LineGrade::Good, LineGrade::Poor) == LineGrade::Poor);
  CHECK(worse(LineGrade::Fair, LineGrade::Good) == LineGrade::Fair);
  CHECK(worse(LineGrade::Unknown, LineGrade::Good) == LineGrade::Good);
  CHECK(worse(LineGrade::Poor, LineGrade::Unknown) == LineGrade::Poor);
  CHECK(worse(LineGrade::Unknown, LineGrade::Unknown) == LineGrade::Unknown);

  // Unstable after two seconds of a poor line, not after a short dip.
  UnstableWatch watch;
  CHECK(!watch.unstable(10, 0));
  watch.report(10, true, 1000);
  CHECK(!watch.unstable(10, 1000) && !watch.unstable(10, 2999));
  watch.report(10, true, 2500);  // still poor: the first moment is kept
  CHECK(watch.unstable(10, 3000));
  watch.report(10, false, 3100);  // better again: at once
  CHECK(!watch.unstable(10, 9000));
  watch.report(10, true, 9000);
  CHECK(!watch.unstable(10, 10999) && watch.unstable(10, 11000));
  // One person's line says nothing about another's.
  watch.report(11, true, 11000);
  CHECK(watch.unstable(10, 11500) && !watch.unstable(11, 11500));
  watch.forget(10);
  CHECK(!watch.unstable(10, 20000) && watch.unstable(11, 20000));
  watch.report(12, true, 11000);
  watch.keepOnly({12, 99});  // who left is forgotten
  CHECK(!watch.unstable(11, 20000) && watch.unstable(12, 20000));
  watch.clear();
  CHECK(!watch.unstable(12, 20000));
}
