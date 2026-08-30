#pragma once

#include "activities/Activity.h"

// Full-screen focus/break timer over the global PomodoroTimer. Idle shows the
// two lengths (Left/Right cycle focus, Up/Down cycle break); running shows a
// large countdown repainted once a minute. Back leaves the screen with the
// timer still going — the reader's status bar carries the remaining time.
class PomodoroActivity final : public Activity {
 public:
  PomodoroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Pomodoro", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  uint32_t lastShownMinute = 0xFFFFFFFF;
  bool announcePhase = false;  // next render follows a phase switch: flash it

  void adjustFocus(int step);
  void adjustBreak(int step);
};
