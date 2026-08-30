#include "CalendarRefreshActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>
#include <WiFi.h>

#include <cstdio>
#include <memory>

#include "CalendarStore.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void CalendarRefreshActivity::onEnter() {
  Activity::onEnter();
  state = SYNCING;
  summary[0] = '\0';

  CALENDAR.ensureLoaded();
  if (!CALENDAR.hasUrl()) {
    state = NO_URL;
    requestUpdate();
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    requestUpdate();
    return;
  }

  shouldTearDownWifiOnExit = true;
  startActivityForResult(
      std::make_unique<WifiSelectionActivity>(renderer, mappedInput, /*autoConnect=*/true),
      [this](const ActivityResult& result) {
        if (result.isCancelled || WiFi.status() != WL_CONNECTED) {
          state = FAILED;
        }
        requestUpdate();
      });
}

void CalendarRefreshActivity::onExit() {
  Activity::onExit();
  if (shouldTearDownWifiOnExit && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void CalendarRefreshActivity::runSync() {
  const int32_t offsetMinutes = (static_cast<int32_t>(SETTINGS.clockUtcOffsetQ) - 48) * 15;
  switch (CALENDAR.refresh(halClock.getEpochSeconds(), offsetMinutes)) {
    case CalendarStore::RefreshResult::Ok:
      snprintf(summary, sizeof(summary), tr(STR_CAL_EVENTS_FMT), static_cast<unsigned>(CALENDAR.eventCount()));
      state = SUCCESS;
      break;
    case CalendarStore::RefreshResult::NoUrl:
      state = NO_URL;
      break;
    default:
      state = FAILED;
      break;
  }
  requestUpdate();
}

void CalendarRefreshActivity::loop() {
  if (state == SYNCING) {
    if (WiFi.status() != WL_CONNECTED) return;  // waiting on the Wi-Fi flow
    requestUpdateAndWait();                     // paint "updating" before the blocking fetch
    runSync();
    return;
  }
  int x = 0;
  int y = 0;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasScreenTapped(x, y)) {
    finish();
  }
}

void CalendarRefreshActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CALENDAR));

  const int midY = pageHeight / 2;
  switch (state) {
    case SYNCING:
      renderer.drawCenteredText(UI_12_FONT_ID, midY, tr(STR_CAL_UPDATING));
      break;
    case SUCCESS:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_CAL_UPDATED_OK), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, summary);
      break;
    case NO_URL:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_CAL_NO_URL), true, EpdFontFamily::BOLD);
      break;
    case FAILED:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_CAL_FAILED), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_CHECK_SERIAL_OUTPUT));
      break;
  }

  if (state != SYNCING) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}
