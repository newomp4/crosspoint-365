#pragma once

#include <I18n.h>

#include "activities/Activity.h"

// Full-screen sleep-screen chooser: Left/Right page through every mode with a
// live preview (Dark, Light, Blank, Year Progress, Reading Heatmap and
// Calendar render for real; file- and state-dependent modes show a described
// placeholder), Confirm keeps the shown one, Back leaves the setting alone.
class SleepScreenPickerActivity final : public Activity {
 public:
  SleepScreenPickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SleepScreenPicker", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int index = 0;

  void drawChip(bool onDark);
  void drawPlaceholder(StrId title, StrId description);
};
