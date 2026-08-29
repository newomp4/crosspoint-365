#include "WeatherRefreshActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "WeatherStore.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void WeatherRefreshActivity::onEnter() {
  Activity::onEnter();
  state = SYNCING;
  summary[0] = '\0';

  WEATHER.ensureLoaded();
  if (!WEATHER.hasLocation()) {
    state = NO_LOCATION;
    if (silent) {
      finish();
      return;
    }
    requestUpdate();
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    requestUpdate();
    return;
  }

  shouldTearDownWifiOnExit = true;
  launchWifiSelection();
}

void WeatherRefreshActivity::onExit() {
  Activity::onExit();

  if (shouldTearDownWifiOnExit && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void WeatherRefreshActivity::launchWifiSelection() {
  // Silent mode must not strand the user in the network list on wake: quiet
  // auto-connect finishes cancelled when no saved network can be joined.
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, /*autoConnect=*/true,
                                                                 /*quietAutoConnect=*/silent),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void WeatherRefreshActivity::onWifiSelectionComplete(const bool connected) {
  if (!connected) {
    LOG_INF("WX", "Wi-Fi not available for weather refresh");
    // Counts as an attempt so a dead network doesn't cost every wake.
    if (silent) WEATHER.noteAttempt(halClock.getEpochSeconds());
    finish();
    return;
  }
  state = SYNCING;
  requestUpdate();
}

void WeatherRefreshActivity::runSync() {
  const uint32_t now = halClock.getEpochSeconds();
  // The Wi-Fi flow refreshes opportunistically on connect; don't fetch twice.
  const bool justRefreshed = WEATHER.hasData() && !WEATHER.isStale(now, 120);
  const auto result = justRefreshed ? WeatherStore::RefreshResult::Ok : WEATHER.refresh(now);
  switch (result) {
    case WeatherStore::RefreshResult::Ok: {
      char temp[12];
      WeatherStore::formatTemperature(WEATHER.temperatureC10(), SETTINGS.weatherUnit, temp, sizeof(temp));
      snprintf(summary, sizeof(summary), "%s \xC2\xB7 %s \xC2\xB7 %s", WEATHER.locationName(), temp,
               I18N.get(WeatherStore::conditionName(WEATHER.weatherCode())));
      state = SUCCESS;
      break;
    }
    case WeatherStore::RefreshResult::NoLocation:
      state = NO_LOCATION;
      break;
    default:
      state = FAILED;
      break;
  }
  if (silent) {
    finish();
    return;
  }
  requestUpdate();
}

void WeatherRefreshActivity::loop() {
  if (state == SYNCING) {
    // Paint the "updating" screen before the blocking fetch.
    requestUpdateAndWait();
    runSync();
    return;
  }
  int x = 0;
  int y = 0;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasScreenTapped(x, y)) {
    finish();
  }
}

void WeatherRefreshActivity::render(RenderLock&&) {
  if (silent) {
    GUI.drawPopup(renderer, tr(STR_WEATHER_UPDATING));
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_HW_WEATHER));

  const int midY = pageHeight / 2;
  switch (state) {
    case SYNCING:
      renderer.drawCenteredText(UI_12_FONT_ID, midY, tr(STR_WEATHER_UPDATING));
      break;
    case SUCCESS:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_WEATHER_UPDATED), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, summary);
      break;
    case NO_LOCATION:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_WEATHER_NO_LOCATION), true, EpdFontFamily::BOLD);
      break;
    case FAILED:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_WEATHER_FAILED), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_CHECK_SERIAL_OUTPUT));
      break;
  }

  if (state != SYNCING) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}
