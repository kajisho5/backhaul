#include "core/aspect_fit.h"

namespace backhaul {

FitResult fit(double srcW, double srcH, double dstW, double dstH, FitMode mode) {
  FitResult r;
  if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return r;

  // Compare shapes by cross-multiplying so equal aspect ratios compare exactly.
  const double lhs = srcW * dstH;  // > rhs: source is wider than the output
  const double rhs = dstW * srcH;

  r.src = {0, 0, srcW, srcH};
  r.dst = {0, 0, dstW, dstH};

  if (lhs == rhs) return r;

  if (mode == FitMode::Letterbox) {
    if (lhs > rhs) {  // wider: bars above and below
      const double h = dstW * srcH / srcW;
      r.dst = {0, (dstH - h) / 2, dstW, h};
    } else {  // taller: bars left and right
      const double w = dstH * srcW / srcH;
      r.dst = {(dstW - w) / 2, 0, w, dstH};
    }
  } else {
    if (lhs > rhs) {  // wider: cut left and right
      const double w = srcH * dstW / dstH;
      r.src = {(srcW - w) / 2, 0, w, srcH};
    } else {  // taller: cut top and bottom
      const double h = srcW * dstH / dstW;
      r.src = {0, (srcH - h) / 2, srcW, h};
    }
  }
  return r;
}

}  // namespace backhaul
