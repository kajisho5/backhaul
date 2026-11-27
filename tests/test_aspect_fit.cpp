#include <initializer_list>

#include "check.h"
#include "core/aspect_fit.h"

using namespace backhaul;

void testAspectFit() {
  const double eps = 1e-9;
  {  // same shape: untouched in both modes, also when only the size differs (720p -> 1080p)
    for (FitMode m : {FitMode::Letterbox, FitMode::CropToFill}) {
      FitResult r = fit(1280, 720, 1920, 1080, m);
      CHECK_NEAR(r.src.x, 0, eps); CHECK_NEAR(r.src.y, 0, eps);
      CHECK_NEAR(r.src.w, 1280, eps); CHECK_NEAR(r.src.h, 720, eps);
      CHECK_NEAR(r.dst.x, 0, eps); CHECK_NEAR(r.dst.y, 0, eps);
      CHECK_NEAR(r.dst.w, 1920, eps); CHECK_NEAR(r.dst.h, 1080, eps);
    }
  }
  {  // 16:10 slide cropped to 16:9: 10% of the height goes, 5% top and 5% bottom
    FitResult r = fit(1920, 1200, 1920, 1080, FitMode::CropToFill);
    CHECK_NEAR(r.src.x, 0, eps); CHECK_NEAR(r.src.y, 60, eps);
    CHECK_NEAR(r.src.w, 1920, eps); CHECK_NEAR(r.src.h, 1080, eps);
    CHECK_NEAR(r.dst.w, 1920, eps); CHECK_NEAR(r.dst.h, 1080, eps);
  }
  {  // 16:10 slide shown whole: bars left and right
    FitResult r = fit(1920, 1200, 1920, 1080, FitMode::Letterbox);
    CHECK_NEAR(r.src.w, 1920, eps); CHECK_NEAR(r.src.h, 1200, eps);
    CHECK_NEAR(r.dst.w, 1728, eps); CHECK_NEAR(r.dst.h, 1080, eps);
    CHECK_NEAR(r.dst.x, 96, eps); CHECK_NEAR(r.dst.y, 0, eps);
  }
  {  // 4:3 camera shown whole
    FitResult r = fit(640, 480, 1920, 1080, FitMode::Letterbox);
    CHECK_NEAR(r.dst.w, 1440, eps); CHECK_NEAR(r.dst.x, 240, eps);
  }
  {  // wider than the output (21:9), shown whole: bars above and below
    FitResult r = fit(2560, 1080, 1920, 1080, FitMode::Letterbox);
    CHECK_NEAR(r.dst.w, 1920, eps); CHECK_NEAR(r.dst.h, 810, eps); CHECK_NEAR(r.dst.y, 135, eps);
  }
  {  // wider than the output, cropped: cut left and right
    FitResult r = fit(2560, 1080, 1920, 1080, FitMode::CropToFill);
    CHECK_NEAR(r.src.w, 1920, eps); CHECK_NEAR(r.src.x, 320, eps); CHECK_NEAR(r.src.h, 1080, eps);
  }
  {  // portrait phone video shown whole
    FitResult r = fit(360, 640, 1920, 1080, FitMode::Letterbox);
    CHECK_NEAR(r.dst.h, 1080, eps); CHECK_NEAR(r.dst.w, 607.5, eps);
  }
  {  // nonsense sizes give empty rectangles instead of dividing by zero
    FitResult r = fit(0, 720, 1920, 1080, FitMode::Letterbox);
    CHECK_NEAR(r.dst.w, 0, eps); CHECK_NEAR(r.src.w, 0, eps);
    r = fit(1280, 720, 1920, -1, FitMode::CropToFill);
    CHECK_NEAR(r.dst.w, 0, eps);
  }
}
