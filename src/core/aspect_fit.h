// Where a picture of one shape goes on an output of another shape.
#pragma once

namespace backhaul {

struct Rect {
  double x = 0, y = 0, w = 0, h = 0;
};

enum class FitMode {
  Letterbox,   // show the whole picture, black bars where it does not reach
  CropToFill,  // fill the output, cut what sticks out (centred)
};

struct FitResult {
  Rect src;  // part of the source picture to use, in source pixels
  Rect dst;  // where it lands on the output, in output pixels
};

// All sizes must be positive. Returns empty rectangles otherwise.
FitResult fit(double srcW, double srcH, double dstW, double dstH, FitMode mode);

}  // namespace backhaul
