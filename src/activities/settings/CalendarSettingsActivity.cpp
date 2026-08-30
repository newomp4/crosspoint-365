#include "CalendarSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "CalendarRefreshActivity.h"
#include "CalendarStore.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
using S = CrossPointSettings;

enum MenuItem { ITEM_BROWSER = 0, ITEM_URL, ITEM_DAYS, ITEM_SLEEP_REFRESH, ITEM_REFRESH_NOW, ITEM_COUNT };
static_assert(ITEM_COUNT == CalendarSettingsActivity::ITEM_COUNT, "keep ITEM_COUNT in sync");

constexpr StrId menuNames[ITEM_COUNT] = {StrId::STR_CAL_BROWSER_SETUP, StrId::STR_CAL_FEED_URL, StrId::STR_CAL_DAYS,
                                         StrId::STR_CAL_SLEEP_REFRESH, StrId::STR_CAL_REFRESH_NOW};
constexpr StrId dayNames[S::CAL_DAYS_COUNT] = {StrId::STR_CAL_DAYS_1, StrId::STR_CAL_DAYS_3, StrId::STR_CAL_DAYS_7};
constexpr StrId refreshNames[S::CAL_SLEEP_REFRESH_COUNT] = {StrId::STR_STATE_OFF, StrId::STR_CAL_REFRESH_10M,
                                                            StrId::STR_CAL_REFRESH_30M, StrId::STR_CAL_REFRESH_1H};
// calendarDays holds the real day count; the popup offers three presets.
constexpr uint8_t dayCounts[S::CAL_DAYS_COUNT] = {1, 3, 7};

uint8_t daysToChoice(const uint8_t days) {
  if (days <= 1) return S::CAL_DAYS_TODAY;
  if (days <= 3) return S::CAL_DAYS_3;
  return S::CAL_DAYS_WEEK;
}
}  // namespace

CalendarSettingsActivity::CalendarSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("CalendarSettings", renderer, mappedInput) {}

void CalendarSettingsActivity::onEnter() {
  UiListActivity::onEnter();
  CALENDAR.ensureLoaded();
  for (int i = 0; i < ITEM_COUNT; i++) {
    rowItems_[i].label = I18N.get(menuNames[i]);
    rowItems_[i].actionValue = static_cast<int16_t>(i);
  }
}

const char* CalendarSettingsActivity::headerTitle() const { return tr(STR_CALENDAR); }

bool CalendarSettingsActivity::handleCustomInput() {
  return optionPopup.handleInput(mappedInput, [this] { requestUpdate(); });
}

void CalendarSettingsActivity::activateIndex(const int index) {
  if (optionPopup.isActive()) return;
  nav.selected = index;
  app.clearTapFlash();
  handleSelection(index);
  requestUpdate();
}

void CalendarSettingsActivity::editUrl() {
  // Opens empty on purpose: a saved secret address wraps to five-plus lines
  // and runs into the keys. Typing replaces the old link; the browser page is
  // the place to see or tweak the saved one.
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_CAL_ENTER_URL), "",
                                                                 CalendarStore::URL_LEN - 1, InputType::Text),
                         [](const ActivityResult& result) {
                           if (result.isCancelled) return;
                           const auto& kb = std::get<KeyboardResult>(result.data);
                           if (kb.text.empty()) return;  // empty confirm keeps the saved link
                           CALENDAR.setFeedUrl(kb.text.c_str());
                         });
}

void CalendarSettingsActivity::handleSelection(const int index) {
  switch (index) {
    case ITEM_BROWSER:
      // The File Transfer web page hosts /calendar: pasting the long secret
      // address in a browser beats typing it on the on-screen keyboard.
      activityManager.goToFileTransfer();
      break;
    case ITEM_URL:
      editUrl();
      break;
    case ITEM_DAYS:
      optionPopup.show(StrId::STR_CAL_DAYS, dayNames, S::CAL_DAYS_COUNT, daysToChoice(SETTINGS.calendarDays),
                       [](const int idx) {
                         SETTINGS.calendarDays = dayCounts[idx < S::CAL_DAYS_COUNT ? idx : 0];
                         SETTINGS.saveToFile();
                       });
      break;
    case ITEM_SLEEP_REFRESH:
      optionPopup.show(StrId::STR_CAL_SLEEP_REFRESH, refreshNames, S::CAL_SLEEP_REFRESH_COUNT,
                       SETTINGS.calendarSleepRefresh < S::CAL_SLEEP_REFRESH_COUNT ? SETTINGS.calendarSleepRefresh : 0,
                       [](const int idx) {
                         SETTINGS.calendarSleepRefresh = static_cast<uint8_t>(idx);
                         SETTINGS.saveToFile();
                       });
      break;
    case ITEM_REFRESH_NOW:
      startActivityForResult(std::make_unique<CalendarRefreshActivity>(renderer, mappedInput), nullptr);
      break;
    default:
      break;
  }
}

std::string CalendarSettingsActivity::rowValueText(const int index) const {
  switch (index) {
    case ITEM_BROWSER:
      return "";
    case ITEM_URL: {
      if (!CALENDAR.hasUrl()) return tr(STR_NOT_SET);
      // Show the feed's host so the row confirms *which* calendar is linked.
      const char* url = CALENDAR.feedUrl();
      const char* start = strstr(url, "://");
      start = start ? start + 3 : url;
      const char* end = strchr(start, '/');
      std::string host = end ? std::string(start, end) : std::string(start);
      if (host.size() > 24) host = host.substr(0, 23) + "\xE2\x80\xA6";
      return host;
    }
    case ITEM_DAYS:
      return I18N.get(dayNames[daysToChoice(SETTINGS.calendarDays)]);
    case ITEM_SLEEP_REFRESH:
      return I18N.get(
          refreshNames[SETTINGS.calendarSleepRefresh < S::CAL_SLEEP_REFRESH_COUNT ? SETTINGS.calendarSleepRefresh : 0]);
    case ITEM_REFRESH_NOW: {
      if (!CALENDAR.hasData()) return "";
      char buf[24];
      snprintf(buf, sizeof(buf), tr(STR_CAL_EVENTS_SHORT), static_cast<unsigned>(CALENDAR.eventCount()));
      return buf;
    }
    default:
      return "";
  }
}

void CalendarSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  for (int i = 0; i < ITEM_COUNT; i++) {
    rowValues_[i] = rowValueText(i);
    rowItems_[i].value = rowValues_[i].empty() ? nullptr : rowValues_[i].c_str();
  }

  fui::ListProps props;
  props.items = rowItems_;
  props.count = static_cast<uint16_t>(ITEM_COUNT);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}

void CalendarSettingsActivity::render(RenderLock&& lock) {
  if (optionPopup.processRender(renderer, mappedInput)) return;
  UiListActivity::render(std::move(lock));
}
