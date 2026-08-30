#pragma once

#include "activities/Activity.h"

// Fetches the iCal feed for the Calendar sleep screen. Reuses the Wi-Fi
// selection flow when the radio is down, reports the outcome, then waits for
// Back — the calendar sibling of WeatherRefreshActivity.
class CalendarRefreshActivity final : public Activity {
 public:
  CalendarRefreshActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("CalendarRefresh", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return true; }
  void render(RenderLock&&) override;

 private:
  enum State { SYNCING, SUCCESS, NO_URL, FAILED };
  State state = SYNCING;
  bool shouldTearDownWifiOnExit = false;
  char summary[48] = {0};

  void runSync();
};
