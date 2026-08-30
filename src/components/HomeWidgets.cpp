#include "HomeWidgets.h"

#include <CivilDate.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>
#include <Icon.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "ReadingStats.h"
#include "RecentBooksStore.h"
#include "WeatherStore.h"
#include "components/DotsScreen.h"
#include "components/UITheme.h"
#include "components/icons/widgetIcons.h"
#include "fontIds.h"

namespace {

constexpr int VALUE_FONT = HELVETICANEUE_14_FONT_ID;  // Bold cut in the regular slot
constexpr int CAPTION_FONT = UI_10_FONT_ID;
constexpr int ICON_SIZE = 24;
constexpr int ICON_GAP = 10;
constexpr int SLOT_GAP = 8;
constexpr int PAD_Y = 6;
constexpr int CAPTION_GAP = 2;
constexpr int TILE_PAD = 12;
constexpr int TILE_GAP = 10;
constexpr int TILE_RADIUS = 14;

constexpr StrId WEEKDAY_SHORT[7] = {StrId::STR_WDS_SUN, StrId::STR_WDS_MON, StrId::STR_WDS_TUE, StrId::STR_WDS_WED,
                                    StrId::STR_WDS_THU, StrId::STR_WDS_FRI, StrId::STR_WDS_SAT};
constexpr StrId WEEKDAY_LONG[7] = {StrId::STR_WD_SUN, StrId::STR_WD_MON, StrId::STR_WD_TUE, StrId::STR_WD_WED,
                                   StrId::STR_WD_THU, StrId::STR_WD_FRI, StrId::STR_WD_SAT};
constexpr StrId MONTH_SHORT[12] = {StrId::STR_MON_JAN, StrId::STR_MON_FEB, StrId::STR_MON_MAR, StrId::STR_MON_APR,
                                   StrId::STR_MON_MAY, StrId::STR_MON_JUN, StrId::STR_MON_JUL, StrId::STR_MON_AUG,
                                   StrId::STR_MON_SEP, StrId::STR_MON_OCT, StrId::STR_MON_NOV, StrId::STR_MON_DEC};

struct Content {
  const freeink::Icon* icon = nullptr;
  char value[24] = {0};
  char caption[40] = {0};
  char captionShort[24] = {0};  // fallback when the caption is wider than the slot
};

uint8_t slotKind(const int slot) {
  const auto& s = SETTINGS;
  const uint8_t kind = slot == 0   ? s.homeWidget1
                       : slot == 1 ? s.homeWidget2
                       : slot == 2 ? s.homeWidget3
                                   : s.homeWidget4;
  return kind < CrossPointSettings::HOME_WIDGET_COUNT ? kind : CrossPointSettings::HW_NONE;
}

const freeink::Icon& weatherIcon(const uint8_t code, const bool daytime) {
  if (code == 0) return daytime ? widget_icon_sun : widget_icon_moon;
  if (code <= 2) return daytime ? widget_icon_cloud_sun : widget_icon_cloud_moon;
  if (code == 3) return widget_icon_cloud;
  if (code == 45 || code == 48) return widget_icon_cloud_fog;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return widget_icon_cloud_rain;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return widget_icon_cloud_snow;
  if (code >= 95) return widget_icon_cloud_lightning;
  return widget_icon_cloud;
}

// "Sat, Aug 29"
void formatShortDate(const CalendarDate& date, char* buf, const size_t bufSize) {
  const uint8_t weekday = CivilDate::weekday(date.epochDay());
  snprintf(buf, bufSize, "%s, %s %u", I18N.get(WEEKDAY_SHORT[weekday]), I18N.get(MONTH_SHORT[date.month - 1]),
           static_cast<unsigned>(date.day));
}

void fill(const uint8_t kind, const CalendarDate& date, Content& c) {
  const auto& s = SETTINGS;
  const int32_t today = date.epochDay();
  switch (kind) {
    case CrossPointSettings::HW_CLOCK: {
      c.icon = &widget_icon_clock;
      if (!halClock.formatTime(c.value, sizeof(c.value), s.clockUtcOffsetQ, s.clockFormat == 1)) {
        snprintf(c.value, sizeof(c.value), "--:--");
        snprintf(c.caption, sizeof(c.caption), "%s", tr(STR_HW_NO_CLOCK));
        break;
      }
      // 12-hour: keep the value compact and move the AM/PM into the caption.
      char period[8] = "";
      if (char* space = strchr(c.value, ' ')) {
        *space = '\0';
        snprintf(period, sizeof(period), "%s \xC2\xB7 ", space + 1);
      }
      if (date.valid) {
        char full[24], brief[16];
        formatShortDate(date, full, sizeof(full));
        snprintf(brief, sizeof(brief), "%s %u", I18N.get(MONTH_SHORT[date.month - 1]), static_cast<unsigned>(date.day));
        snprintf(c.caption, sizeof(c.caption), "%s%s", period, full);
        snprintf(c.captionShort, sizeof(c.captionShort), "%s%s", period, brief);
      } else if (period[0]) {
        period[strlen(period) - 4] = '\0';  // drop the separator
        snprintf(c.caption, sizeof(c.caption), "%s", period);
      }
      break;
    }
    case CrossPointSettings::HW_DATE:
      c.icon = &widget_icon_calendar;
      if (date.valid) {
        snprintf(c.value, sizeof(c.value), "%s %u", I18N.get(MONTH_SHORT[date.month - 1]),
                 static_cast<unsigned>(date.day));
        snprintf(c.caption, sizeof(c.caption), "%s", I18N.get(WEEKDAY_LONG[CivilDate::weekday(today)]));
      } else {
        snprintf(c.value, sizeof(c.value), "--");
        snprintf(c.caption, sizeof(c.caption), "%s", tr(STR_HW_NO_CLOCK));
      }
      break;
    case CrossPointSettings::HW_TODAY:
      c.icon = &widget_icon_book_open;
      ReadingStats::formatDuration(date.valid ? READING_STATS.secondsOn(today) : 0, c.value, sizeof(c.value));
      snprintf(c.caption, sizeof(c.caption), "%s", tr(STR_HW_CAP_TODAY));
      break;
    case CrossPointSettings::HW_WEEK: {
      c.icon = &widget_icon_calendar_dots;
      const auto week = date.valid ? READING_STATS.summarize(today - 6, today) : ReadingStats::Summary{};
      ReadingStats::formatDuration(week.totalSeconds, c.value, sizeof(c.value));
      snprintf(c.caption, sizeof(c.caption), "%s", tr(STR_HW_CAP_WEEK));
      break;
    }
    case CrossPointSettings::HW_TOTAL:
      c.icon = &widget_icon_books;
      ReadingStats::formatDuration(READING_STATS.allTimeSeconds(), c.value, sizeof(c.value));
      snprintf(c.caption, sizeof(c.caption), "%s", tr(STR_HW_CAP_TOTAL));
      break;
    case CrossPointSettings::HW_BOOK: {
      c.icon = &widget_icon_book_bookmark;
      const std::string& path = APP_STATE.openEpubPath;
      ReadingStats::formatDuration(READING_STATS.secondsForBook(path.c_str()), c.value, sizeof(c.value));
      const auto& books = RECENT_BOOKS.getBooks();
      if (!books.empty() && books.front().path == path && !books.front().title.empty()) {
        snprintf(c.caption, sizeof(c.caption), "%s", books.front().title.c_str());
      } else {
        snprintf(c.caption, sizeof(c.caption), "%s", tr(STR_HW_CAP_BOOK));
      }
      break;
    }
    case CrossPointSettings::HW_STREAK: {
      c.icon = &widget_icon_fire;
      const auto recent = date.valid ? READING_STATS.summarize(today - 400, today) : ReadingStats::Summary{};
      snprintf(c.value, sizeof(c.value), "%u", static_cast<unsigned>(recent.streak));
      snprintf(c.caption, sizeof(c.caption), "%s", tr(STR_HW_CAP_STREAK));
      break;
    }
    case CrossPointSettings::HW_AVERAGE: {
      c.icon = &widget_icon_chart_bar;
      const auto month = date.valid ? READING_STATS.summarize(today - 29, today) : ReadingStats::Summary{};
      ReadingStats::formatDuration(month.totalSeconds / 30, c.value, sizeof(c.value));
      snprintf(c.caption, sizeof(c.caption), "%s", tr(STR_HW_CAP_AVERAGE));
      break;
    }
    case CrossPointSettings::HW_YEAR:
      c.icon = &widget_icon_hourglass;
      if (date.valid) {
        snprintf(c.value, sizeof(c.value), "%u%%", static_cast<unsigned>(date.dayOfYear() * 100u / date.daysInYear()));
        snprintf(c.caption, sizeof(c.caption), tr(STR_HW_CAP_YEAR), static_cast<unsigned>(date.year));
      } else {
        snprintf(c.value, sizeof(c.value), "--%%");
        snprintf(c.caption, sizeof(c.caption), "%s", tr(STR_HW_NO_CLOCK));
      }
      break;
    case CrossPointSettings::HW_WEATHER: {
      WEATHER.ensureLoaded();
      if (!WEATHER.hasLocation()) {
        c.icon = &widget_icon_cloud;
        snprintf(c.value, sizeof(c.value), "--");
        snprintf(c.caption, sizeof(c.caption), "%s", tr(STR_HW_SET_LOCATION));
      } else if (!WEATHER.hasData()) {
        c.icon = &widget_icon_cloud;
        snprintf(c.value, sizeof(c.value), "--");
        snprintf(c.caption, sizeof(c.caption), "%s", tr(STR_HW_NO_WEATHER));
      } else {
        c.icon = &weatherIcon(WEATHER.weatherCode(), WEATHER.isDaytime());
        WeatherStore::formatTemperature(WEATHER.temperatureC10(), s.weatherUnit, c.value, sizeof(c.value));
        char hi[12], lo[12];
        WeatherStore::formatTemperature(WEATHER.maxC10(), s.weatherUnit, hi, sizeof(hi));
        WeatherStore::formatTemperature(WEATHER.minC10(), s.weatherUnit, lo, sizeof(lo));
        snprintf(c.caption, sizeof(c.caption), "%s \xC2\xB7 %s/%s",
                 I18N.get(WeatherStore::conditionName(WEATHER.weatherCode())), hi, lo);
        snprintf(c.captionShort, sizeof(c.captionShort), "%s/%s", hi, lo);
      }
      break;
    }
    case CrossPointSettings::HW_BATTERY:
      c.icon = &widget_icon_battery;
      snprintf(c.value, sizeof(c.value), "%u%%", static_cast<unsigned>(powerManager.getBatteryPercentage()));
      snprintf(c.caption, sizeof(c.caption), "%s", tr(STR_HW_CAP_BATTERY));
      break;
    default:
      break;
  }
}

// freeink::Icon blit, upright (unlike the legacy pre-rotated drawIcon bitmaps).
void drawIcon(const GfxRenderer& renderer, const freeink::Icon& icon, const int x, const int y) {
  const int rowBytes = (icon.w + 7) / 8;
  for (int row = 0; row < icon.h; row++) {
    const uint8_t* line = icon.bits + row * rowBytes;
    for (int col = 0; col < icon.w; col++) {
      if (((line[col >> 3] >> (7 - (col & 7))) & 1) == 0) renderer.drawPixel(x + col, y + row, true);
    }
  }
}

}  // namespace

int HomeWidgets::slotCount() {
  int n = 0;
  for (int i = 0; i < CrossPointSettings::HOME_WIDGET_SLOTS; i++) {
    if (slotKind(i) != CrossPointSettings::HW_NONE) n++;
  }
  return n;
}

bool HomeWidgets::showsClock() {
  for (int i = 0; i < CrossPointSettings::HOME_WIDGET_SLOTS; i++) {
    const uint8_t k = slotKind(i);
    if (k == CrossPointSettings::HW_CLOCK || k == CrossPointSettings::HW_DATE) return true;
  }
  return false;
}

bool HomeWidgets::showsWeather() {
  for (int i = 0; i < CrossPointSettings::HOME_WIDGET_SLOTS; i++) {
    if (slotKind(i) == CrossPointSettings::HW_WEATHER) return true;
  }
  return false;
}

namespace {
// Four widgets go in two rows of two: a 480 px screen split four ways leaves
// no room for a value like "2h 10m" at the value font's size.
int rowsFor(const int slots) { return slots == 4 ? 2 : 1; }

int rowHeight(const GfxRenderer& renderer, const bool compact) {
  const int valueLine = std::max(renderer.getLineHeight(VALUE_FONT), ICON_SIZE);
  if (compact) return PAD_Y + valueLine + PAD_Y;
  return PAD_Y + valueLine + CAPTION_GAP + renderer.getLineHeight(CAPTION_FONT) + PAD_Y;
}

// Longest caption that fits: the full one, its short form, or the full one truncated.
std::string fitCaption(const GfxRenderer& renderer, const Content& c, const int width) {
  if (renderer.getTextWidth(CAPTION_FONT, c.caption) <= width) return c.caption;
  if (c.captionShort[0] && renderer.getTextWidth(CAPTION_FONT, c.captionShort) <= width) return c.captionShort;
  return renderer.truncatedText(CAPTION_FONT, c.caption, width);
}

int tileHeight(const GfxRenderer& renderer, const bool compact) {
  const int valueLine = std::max(renderer.getLineHeight(VALUE_FONT), ICON_SIZE);
  if (compact) return TILE_PAD + valueLine + TILE_PAD;
  return TILE_PAD + valueLine + CAPTION_GAP + renderer.getLineHeight(CAPTION_FONT) + TILE_PAD;
}

bool tileStyle() { return UITheme::getInstance().getMetrics().homeWidgetTiles; }

void drawTiles(const GfxRenderer& renderer, const Rect& band, const bool compact, const int n,
               const CalendarDate& date) {
  const int side = UITheme::getInstance().getMetrics().contentSidePadding;
  const int rows = (n + 1) / 2;
  const int tileH = tileHeight(renderer, compact);
  const int fullWidth = band.width - 2 * side;
  const int halfWidth = (fullWidth - TILE_GAP) / 2;
  const int valueLine = std::max(renderer.getLineHeight(VALUE_FONT), ICON_SIZE);
  const int valueOffset = TILE_PAD + (valueLine - renderer.getLineHeight(VALUE_FONT)) / 2;
  const int iconOffset = TILE_PAD + (valueLine - ICON_SIZE) / 2;
  const int captionOffset = TILE_PAD + valueLine + CAPTION_GAP;

  int slot = 0;
  for (int i = 0; i < CrossPointSettings::HOME_WIDGET_SLOTS && slot < n; i++) {
    const uint8_t kind = slotKind(i);
    if (kind == CrossPointSettings::HW_NONE) continue;
    Content c;
    fill(kind, date, c);
    const int row = slot / 2;
    const int col = slot % 2;
    // An odd last tile spans the row, like a dashboard's wide stat card.
    const bool wide = (slot == n - 1) && (col == 0);
    const int width = wide ? fullWidth : halfWidth;
    const int x = band.x + side + col * (halfWidth + TILE_GAP);
    const int y = band.y + row * (tileH + TILE_GAP);
    renderer.fillRoundedRect(x, y, width, tileH, TILE_RADIUS, Color::LightGray);
    const int textX = x + TILE_PAD + ICON_SIZE + ICON_GAP;
    const int textWidth = width - (textX - x) - TILE_PAD;
    if (c.icon) drawIcon(renderer, *c.icon, x + TILE_PAD, y + iconOffset);
    if (c.value[0]) {
      const std::string value = renderer.truncatedText(VALUE_FONT, c.value, textWidth);
      renderer.drawText(VALUE_FONT, textX, y + valueOffset, value.c_str(), true);
    }
    if (!compact && c.caption[0]) {
      const std::string caption = fitCaption(renderer, c, width - 2 * TILE_PAD);
      renderer.drawText(CAPTION_FONT, x + TILE_PAD, y + captionOffset, caption.c_str(), true);
    }
    slot++;
  }
  (void)rows;
}
}  // namespace

int HomeWidgets::bandHeight(const GfxRenderer& renderer, const bool compact) {
  const int n = slotCount();
  if (tileStyle()) {
    const int rows = (n + 1) / 2;
    return rows * tileHeight(renderer, compact) + rows * TILE_GAP;  // trailing gap keeps the card below clear
  }
  const int rows = rowsFor(n);
  return rows * rowHeight(renderer, compact) - (rows - 1) * PAD_Y;
}

bool HomeWidgets::formatHeaderDate(char* buf, const size_t bufSize) {
  const CalendarDate date = CalendarDate::today();
  if (!date.valid) return false;
  snprintf(buf, bufSize, "%s, %s %u", I18N.get(WEEKDAY_LONG[CivilDate::weekday(date.epochDay())]),
           I18N.get(MONTH_SHORT[date.month - 1]), static_cast<unsigned>(date.day));
  return true;
}

void HomeWidgets::draw(const GfxRenderer& renderer, const Rect& band, const bool compact) {
  const int n = slotCount();
  if (n == 0) return;
  if (tileStyle()) {
    drawTiles(renderer, band, compact, n, CalendarDate::today());
    return;
  }
  const int rows = rowsFor(n);
  const int cols = (n + rows - 1) / rows;
  const int side = UITheme::getInstance().getMetrics().contentSidePadding;
  const int rowStride = rowHeight(renderer, compact) - PAD_Y;  // adjacent rows share one padding
  const int valueLine = std::max(renderer.getLineHeight(VALUE_FONT), ICON_SIZE);
  const int valueOffset = PAD_Y + (valueLine - renderer.getLineHeight(VALUE_FONT)) / 2;
  const int iconOffset = PAD_Y + (valueLine - ICON_SIZE) / 2;
  const int captionOffset = PAD_Y + valueLine + CAPTION_GAP;
  const CalendarDate date = CalendarDate::today();

  // Widths are content-driven ("12:58" and "128h" need different room), so
  // each row is laid out like a flex row: every slot starts at its natural
  // width and spare space is shared evenly. When a row overflows, the widest
  // slot gives way first — its short caption, then clipping — so one long
  // caption never squeezes its neighbours.
  constexpr int SLOTS = CrossPointSettings::HOME_WIDGET_SLOTS;
  Content contents[SLOTS];
  int fullWidth[SLOTS] = {0};
  int shortWidth[SLOTS] = {0};
  bool useShort[SLOTS] = {false};
  int slot = 0;
  for (int i = 0; i < SLOTS && slot < n; i++) {
    const uint8_t kind = slotKind(i);
    if (kind == CrossPointSettings::HW_NONE) continue;
    Content& c = contents[slot];
    fill(kind, date, c);
    const int valueWidth = c.value[0] ? renderer.getTextWidth(VALUE_FONT, c.value) : 0;
    const int captionWidth = (!compact && c.caption[0]) ? renderer.getTextWidth(CAPTION_FONT, c.caption) : 0;
    const int shortCaptionWidth =
        (!compact && c.captionShort[0]) ? renderer.getTextWidth(CAPTION_FONT, c.captionShort) : captionWidth;
    fullWidth[slot] = ICON_SIZE + ICON_GAP + std::max(valueWidth, captionWidth);
    shortWidth[slot] = ICON_SIZE + ICON_GAP + std::max(valueWidth, std::min(captionWidth, shortCaptionWidth));
    slot++;
  }

  const int available = band.width - 2 * side - (cols - 1) * SLOT_GAP;
  for (int row = 0; row < rows; row++) {
    const int first = row * cols;
    const int inRow = std::min(cols, n - first);
    int width[SLOTS];
    for (int k = 0; k < inRow; k++) width[k] = fullWidth[first + k];
    for (;;) {
      int sum = 0;
      int widest = 0;
      for (int k = 0; k < inRow; k++) {
        sum += width[k];
        if (width[k] > width[widest]) widest = k;
      }
      if (sum <= available) break;
      if (!useShort[first + widest] && shortWidth[first + widest] < width[widest]) {
        useShort[first + widest] = true;
        width[widest] = shortWidth[first + widest];
        continue;
      }
      // Nothing left to shorten: the widest slot takes what remains and clips.
      width[widest] = std::max(ICON_SIZE + ICON_GAP, available - (sum - width[widest]));
      break;
    }
    int sum = 0;
    for (int k = 0; k < inRow; k++) sum += width[k];
    const int spare = std::max(0, available - sum) / inRow;

    int x = band.x + side;
    const int y = band.y + row * rowStride;
    for (int k = 0; k < inRow; k++) {
      const Content& c = contents[first + k];
      const int textWidth = width[k] + spare - ICON_SIZE - ICON_GAP;
      const int textX = x + ICON_SIZE + ICON_GAP;
      if (c.icon) drawIcon(renderer, *c.icon, x, y + iconOffset);
      if (c.value[0]) {
        const std::string value = renderer.truncatedText(VALUE_FONT, c.value, textWidth);
        renderer.drawText(VALUE_FONT, textX, y + valueOffset, value.c_str(), true);
      }
      if (!compact && c.caption[0]) {
        const std::string caption = fitCaption(renderer, c, textWidth);
        renderer.drawText(CAPTION_FONT, textX, y + captionOffset, caption.c_str(), true);
      }
      x += width[k] + spare + SLOT_GAP;
    }
  }
}

// Reading-activity panel: one bar per day for the last two weeks, on the same
// dithered card as the widget tiles. Fills the flexible space between the
// continue-reading card and a bottom-pinned menu (Mono).
void HomeWidgets::drawActivityPanel(const GfxRenderer& renderer, const Rect& rect) {
  constexpr int DAYS = 14;
  constexpr int BAR_GAP = 6;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int side = metrics.contentSidePadding;
  const int x = rect.x + side;
  const int width = rect.width - 2 * side;
  renderer.fillRoundedRect(x, rect.y, width, rect.height, TILE_RADIUS, Color::LightGray);

  READING_STATS.ensureLoaded();
  const int32_t today = CalendarDate::today().epochDay();
  uint32_t seconds[DAYS];
  uint32_t total = 0;
  uint32_t maxSeconds = 0;
  for (int i = 0; i < DAYS; i++) {
    seconds[i] = READING_STATS.secondsOn(today - (DAYS - 1 - i));
    total += seconds[i];
    maxSeconds = std::max(maxSeconds, seconds[i]);
  }

  const int captionLine = renderer.getLineHeight(CAPTION_FONT);
  renderer.drawText(CAPTION_FONT, x + TILE_PAD + 2, rect.y + TILE_PAD, tr(STR_HW_LAST_14), true);
  char totalText[16];
  ReadingStats::formatDuration(total, totalText, sizeof(totalText));
  const int totalWidth = renderer.getTextWidth(VALUE_FONT, totalText);
  const int valueLine = renderer.getLineHeight(VALUE_FONT);
  renderer.drawText(VALUE_FONT, x + width - TILE_PAD - 2 - totalWidth,
                    rect.y + TILE_PAD + (captionLine - valueLine) / 2, totalText, true);

  const int chartTop = rect.y + TILE_PAD + std::max(captionLine, valueLine) + 8;
  const int chartBottom = rect.y + rect.height - TILE_PAD;
  const int chartHeight = chartBottom - chartTop;
  if (chartHeight < 12) return;
  const int barWidth = (width - 2 * (TILE_PAD + 2) - (DAYS - 1) * BAR_GAP) / DAYS;
  if (barWidth < 4) return;
  const int chartX = x + (width - DAYS * barWidth - (DAYS - 1) * BAR_GAP) / 2;
  for (int i = 0; i < DAYS; i++) {
    const int bx = chartX + i * (barWidth + BAR_GAP);
    int barHeight = 3;
    if (maxSeconds > 0 && seconds[i] > 0) {
      barHeight = std::max(4, static_cast<int>(static_cast<uint64_t>(chartHeight) * seconds[i] / maxSeconds));
    }
    const int radius = std::min(3, barWidth / 2);
    if (seconds[i] > 0) {
      renderer.fillRoundedRect(bx, chartBottom - barHeight, barWidth, barHeight, radius, Color::Black);
    } else {
      // Empty day: a low stub so the axis stays readable on the dither.
      renderer.fillRoundedRect(bx, chartBottom - barHeight, barWidth, barHeight, 1, Color::DarkGray);
    }
  }
}
