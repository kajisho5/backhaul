// What this PC's Windows says about power and notifications, and keeping the PC awake.
// Everything that touches Windows for these lives in system_facts.cpp; elsewhere the
// answers are "unknown" and keeping awake does nothing.
#pragma once

struct SystemFacts {
  int power = -1;          // -1 unknown, 0 on battery, 1 on mains
  int notifications = -1;  // -1 unknown, 0 Windows holds notifications back right now, 1 it shows them
};

SystemFacts readSystemFacts();

// True where keepAwake() does something.
bool canKeepAwake();

// While on, Windows neither goes to sleep nor switches the screens off by itself. It
// cannot stop the lid or the power button, and it does not stop a screen saver.
// Call from the main thread only (Windows keeps this per thread).
void keepAwake(bool on);
