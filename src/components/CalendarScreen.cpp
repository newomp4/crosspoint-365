#include "CalendarScreen.h"

#include <CivilDate.h>
#include <HalClock.h>
#include <HalDisplay.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "CalendarStore.h"
#include "CrossPointSettings.h"
#include "components/DotsScreen.h"
#include "fontIds.h"

namespace {
constexpr int SIDE = 24;
constexpr int TOP = 28;
constexpr int TIME_COLUMN = 104;
constexpr int DAY_GAP = 18;
constexpr int EVENT_GAP = 6;

constexpr StrId WEEKDAY_LONG[7] = {StrId::STR_WD_SUN, StrId::STR_WD_MON, StrId::STR_WD_TUE, StrId::STR_WD_WED,
                                   StrId::STR_WD_THU, StrId::STR_WD_FRI, StrId::STR_WD_SAT};
constexpr StrId MONTH_SHORT[12] = {StrId::STR_MON_JAN, StrId::STR_MON_FEB, StrId::STR_MON_MAR, StrId::STR_MON_APR,
                                   StrId::STR_MON_MAY, StrId::STR_MON_JUN, StrId::STR_MON_JUL, StrId::STR_MON_AUG,
                                   StrId::STR_MON_SEP, StrId::STR_MON_OCT, StrId::STR_MON_NOV, StrId::STR_MON_DEC};

void formatClock(const uint32_t localMinute, char* buf, const size_t bufSize) {
  const unsigned hour24 = (localMinute / 60) % 24;
  const unsigned minute = localMinute % 60;
  if (SETTINGS.clockFormat == 1) {
    unsigned hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(buf, bufSize, "%u:%02u%s", hour12, minute, hour24 >= 12 ? "p" : "a");
  } else {
    snprintf(buf, bufSize, "%02u:%02u", hour24, minute);
  }
}

// "Today", "Tomorrow", then "Monday, Sep 1"
void formatDayHeading(const int32_t day, const int32_t today, char* buf, const size_t bufSize) {
  if (day == today) {
    snprintf(buf, bufSize, "%s", tr(STR_CAL_TODAY));
    return;
  }
  if (day == today + 1) {
    snprintf(buf, bufSize, "%s", tr(STR_CAL_TOMORROW));
    return;
  }
  uint16_t y;
  uint8_t m, d;
  CivilDate::civilFromDays(day, y, m, d);
  snprintf(buf, bufSize, "%s, %s %u", I18N.get(WEEKDAY_LONG[CivilDate::weekday(day)]), I18N.get(MONTH_SHORT[m - 1]),
           static_cast<unsigned>(d));
}
}  // namespace

void CalendarScreen::render(GfxRenderer& renderer) {
  CALENDAR.ensureLoaded();
  const auto savedOrientation = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  renderer.setRenderMode(GfxRenderer::BW);
  renderer.clearScreen();

  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  const CalendarDate date = CalendarDate::today();
  const int32_t offsetMinutes = (static_cast<int32_t>(SETTINGS.clockUtcOffsetQ) - 48) * 15;
  const uint32_t nowEpoch = halClock.getEpochSeconds();
  const uint32_t nowLocalMinute = nowEpoch / 60 + offsetMinutes;
  const int32_t today = date.valid ? date.epochDay() : 0;

  // Headline: the date, with the fetch time at the right.
  const int titleLh = renderer.getLineHeight(UI_TITLE_FONT_ID);
  char heading[40];
  if (date.valid) {
    snprintf(heading, sizeof(heading), "%s, %s %u", I18N.get(WEEKDAY_LONG[CivilDate::weekday(today)]),
             I18N.get(MONTH_SHORT[date.month - 1]), static_cast<unsigned>(date.day));
  } else {
    snprintf(heading, sizeof(heading), "%s", tr(STR_DOTS_CLOCK_NOT_SET));
  }
  renderer.drawText(UI_TITLE_FONT_ID, SIDE, TOP, heading, true);
  int y = TOP + titleLh + 4;
  if (CALENDAR.hasData() && CALENDAR.fetchedAtEpoch() > 0) {
    char updated[24], clock[12];
    formatClock(CALENDAR.fetchedAtEpoch() / 60 + offsetMinutes, clock, sizeof(clock));
    snprintf(updated, sizeof(updated), tr(STR_CAL_UPDATED), clock);
    renderer.drawText(SMALL_FONT_ID, SIDE, y, updated, true);
  }
  y += renderer.getLineHeight(SMALL_FONT_ID) + DAY_GAP;

  const int dayLh = renderer.getLineHeight(UI_10_FONT_ID);
  const int eventLh = renderer.getLineHeight(UI_12_FONT_ID);
  const int bottom = height - 24;

  if (!CALENDAR.hasUrl()) {
    renderer.drawText(UI_12_FONT_ID, SIDE, y, tr(STR_CAL_NO_URL), true);
    renderer.setOrientation(savedOrientation);
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    return;
  }

  // Day-structured view: every day in range gets its heading, whether or not
  // anything is scheduled — an empty calendar still looks like a calendar.
  const int limitDays = std::clamp<int>(SETTINGS.calendarDays, 1, CalendarStore::WINDOW_DAYS);
  if (!CALENDAR.hasData()) {
    renderer.drawText(UI_12_FONT_ID, SIDE, y, tr(STR_CAL_NOT_FETCHED), true, EpdFontFamily::BOLD);
    y += eventLh + 4;
    renderer.drawText(UI_10_FONT_ID, SIDE, y, tr(STR_CAL_FETCH_HINT), true);
  } else {
    uint8_t next = 0;
    for (int d = 0; d < limitDays; d++) {
      const int32_t day = today + d;
      if (y + dayLh + eventLh > bottom) break;
      if (d > 0) y += DAY_GAP;
      char dayHeading[40];
      formatDayHeading(day, today, dayHeading, sizeof(dayHeading));
      renderer.drawText(UI_10_FONT_ID, SIDE, y, dayHeading, true, EpdFontFamily::BOLD);
      y += dayLh + 4;
      renderer.fillRectDither(SIDE, y, width - 2 * SIDE, 1, Color::DarkGray);
      y += 8;

      int shownForDay = 0;
      for (; next < CALENDAR.eventCount(); next++) {
        const auto& e = CALENDAR.event(next);
        if (e.endMinute() <= nowLocalMinute && !(e.allDay && e.startMinute / 1440 == static_cast<uint32_t>(today))) {
          continue;  // already over
        }
        // An event still running from an earlier day files under today.
        const int32_t eventDay = std::max<int32_t>(static_cast<int32_t>(e.startMinute / 1440), today);
        if (eventDay != day) break;
        if (y + eventLh > bottom) {
          char more[24];
          snprintf(more, sizeof(more), tr(STR_CAL_MORE), static_cast<unsigned>(CALENDAR.eventCount() - next));
          renderer.drawText(SMALL_FONT_ID, SIDE, y, more, true);
          y = bottom;
          break;
        }
        // Start time only: a start-end range does not fit TIME_COLUMN in the UI font.
        char timeText[24];
        if (e.allDay) {
          snprintf(timeText, sizeof(timeText), "%s", tr(STR_CAL_ALL_DAY));
        } else {
          formatClock(e.startMinute, timeText, sizeof(timeText));
        }
        const bool now = !e.allDay && e.startMinute <= nowLocalMinute && e.endMinute() > nowLocalMinute;
        if (now) {
          // Happening-now marker: a small ink bar in the left margin.
          renderer.fillRoundedRect(SIDE - 12, y + 3, 4, eventLh - 6, 2, Color::Black);
        }
        renderer.drawText(UI_10_FONT_ID, SIDE, y + (eventLh - dayLh) / 2, timeText, true,
                          now ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
        const int textX = SIDE + TIME_COLUMN;
        const std::string title = renderer.truncatedText(UI_12_FONT_ID, e.summary, width - SIDE - textX);
        renderer.drawText(UI_12_FONT_ID, textX, y, title.c_str(), true,
                          now ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
        y += eventLh + EVENT_GAP;
        shownForDay++;
      }
      if (shownForDay == 0 && y + dayLh <= bottom) {
        renderer.drawText(UI_10_FONT_ID, SIDE, y, tr(STR_CAL_FREE), true);
        y += dayLh + EVENT_GAP;
      }
    }
  }

  renderer.setOrientation(savedOrientation);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
