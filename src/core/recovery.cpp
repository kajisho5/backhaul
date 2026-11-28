#include "core/recovery.h"

namespace backhaul {

bool shouldRejoin(EndCause cause) { return cause == EndCause::Connection; }

int rejoinWaitSeconds(int attempt) {
  if (attempt <= 1) return 5;
  if (attempt == 2) return 10;
  if (attempt == 3) return 20;
  return 30;
}

}  // namespace backhaul
