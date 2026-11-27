// How much video quality to ask Zoom for.
//
// The Zoom Meeting SDK caps what one connection may receive at the same time:
// one 1080p stream, or two 720p streams, or 34 "units" of lower streams
// (360p = 8 units, 180p = 2 units, 90p = 1 unit). 1080p and 720p cannot be mixed.
// Source: https://developers.zoom.us/blog/concurrent-video-streaming-limits-desktop-sdks/
#pragma once

namespace backhaul {

enum class Resolution { R90, R180, R360, R720, R1080 };

// Operator setting: "auto" asks for the best allowed, "max720" never asks above 720p.
enum class QualityCap { Auto, Max720 };

// Picture height in pixels (90, 180, 360, 720, 1080).
int heightOf(Resolution r);

// How many streams of this resolution one connection may receive at once.
int maxSimultaneous(Resolution r);

// Best resolution to request for every person when `personCount` people are
// received at once. Returns R90 when there are more people than even 90p allows;
// the caller must then limit the count with maxSimultaneous(R90).
Resolution pickPersonResolution(int personCount, QualityCap cap);

}  // namespace backhaul
