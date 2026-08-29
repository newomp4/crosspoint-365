#pragma once

#include "activities/Activity.h"

// Fetches the weather for the home widget. Reuses the Wi-Fi selection flow
// when the radio is down, reports the outcome, then waits for Back. In silent
// mode (auto-refresh on wake) it shows only a popup and finishes on its own.
class WeatherRefreshActivity final : public Activity {
 public:
  WeatherRefreshActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const bool silent = false)
      : Activity("WeatherRefresh", renderer, mappedInput), silent(silent) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return true; }
  void render(RenderLock&&) override;

 private:
  enum State { SYNCING, SUCCESS, NO_LOCATION, FAILED };
  const bool silent;
  State state = SYNCING;
  bool shouldTearDownWifiOnExit = false;
  char summary[48] = {0};

  void runSync();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
};
