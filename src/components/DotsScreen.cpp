#include "DotsScreen.h"

#include <CivilDate.h>
#include <HalClock.h>
#include <HalDisplay.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "CrossPointSettings.h"
#include "ReadingStats.h"
#include "fontIds.h"

namespace {

// Dot diameter as a percentage of the grid cell, indexed by DOTS_SIZE.
constexpr uint8_t DOT_SCALE_PERCENT[CrossPointSettings::DOTS_SIZE_COUNT] = {25, 35, 45, 55, 65, 80};
// Outer screen margin in px, indexed by DOTS_SCALE (Small / Medium / Large).
constexpr uint8_t MARGIN_PX[CrossPointSettings::DOTS_SCALE_COUNT] = {24, 40, 64};
constexpr int MONTH_COLUMNS = 31;
constexpr int WEEK_COLUMNS = 7;
constexpr int TITLE_SUBTITLE_GAP = 4;
constexpr int GRID_TEXT_GAP = 28;
constexpr size_t TEXT_BUF = 48;
// Calendar used for the year grid when no real date is available.
constexpr uint16_t FALLBACK_YEAR = 2025;
// Heatmap shade thresholds (minutes) for the fixed scales: {dark gray, black}.
constexpr uint16_t HEAT_THRESHOLDS[CrossPointSettings::HEAT_SCALE_COUNT][2] = {{0, 0}, {10, 30}, {20, 60}, {45, 120}};

constexpr StrId MONTH_NAMES[12] = {StrId::STR_MON_JAN, StrId::STR_MON_FEB, StrId::STR_MON_MAR, StrId::STR_MON_APR,
                                   StrId::STR_MON_MAY, StrId::STR_MON_JUN, StrId::STR_MON_JUL, StrId::STR_MON_AUG,
                                   StrId::STR_MON_SEP, StrId::STR_MON_OCT, StrId::STR_MON_NOV, StrId::STR_MON_DEC};

// Pixel value as the grayscale planes see it (same convention as Bitmap and
// the 2-bit fonts): 0 black, 1 dark gray, 2 light gray, 3 white.
enum class Level : uint8_t { Black = 0, DarkGray = 1, LightGray = 2, White = 3 };

enum class Pass : uint8_t { BW, GrayLsb, GrayMsb };

// Resolved look of one dot category.
struct DotClass {
  bool visible = true;
  bool outline = false;  // contrast ring around a background-colored core
  Level level = Level::Black;
};

struct TextLine {
  char text[TEXT_BUF] = {0};
  int fontId = 0;
  EpdFontFamily::Style style = EpdFontFamily::REGULAR;
  int height = 0;  // line height; 0 when hidden
  int y = 0;
};

struct Layout {
  int cols = 0;
  int rows = 0;
  int cell = 0;
  int dot = 0;
  int gridX = 0;
  int gridY = 0;
  int gridW = 0;
  int gridH = 0;
  TextLine title;
  TextLine subtitle;
};

// Everything a draw pass needs, resolved once per render.
struct Context {
  const GfxRenderer* renderer = nullptr;
  CalendarDate date;
  Layout layout;
  bool blackBackground = false;
  uint8_t shape = CrossPointSettings::DOTS_SHAPE_CIRCLE;

  // Year Progress
  DotClass past, today, future;
  int todayIndex = -1;  // 0-based day index; -1 without a valid date
  int totalDays = 365;
  bool byMonth = false;

  // Reading Heatmap
  int32_t firstDay = 0;  // epoch days, inclusive
  int32_t lastDay = 0;
  int days = 0;
  int firstOffset = 0;  // blank cells before the first day (week layout)
  bool byWeek = true;
  uint32_t darkSeconds = 0;  // shade thresholds resolved from the scale setting
  uint32_t blackSeconds = 0;
  DotClass empty;
  ReadingStats::Summary summary;
};

using DrawFn = void (*)(const Context&, Pass);

uint8_t clampIndex(const uint8_t value, const uint8_t count) { return value < count ? value : 0; }

int fontIdFor(const uint8_t font, const uint8_t size, EpdFontFamily::Style& style) {
  static constexpr int HELVETICA[CrossPointSettings::DOTS_SCALE_COUNT] = {
      HELVETICANEUE_14_FONT_ID, HELVETICANEUE_24_FONT_ID, HELVETICANEUE_40_FONT_ID};
  static constexpr int GEIST[CrossPointSettings::DOTS_SCALE_COUNT] = {GEIST_14_FONT_ID, GEIST_24_FONT_ID,
                                                                      GEIST_40_FONT_ID};
  const uint8_t sizeIdx = clampIndex(size, CrossPointSettings::DOTS_SCALE_COUNT);
  switch (font) {
    case CrossPointSettings::DOTS_FONT_GEIST_BOLD:
      style = EpdFontFamily::BOLD;
      return GEIST[sizeIdx];
    case CrossPointSettings::DOTS_FONT_GEIST_MEDIUM:
      style = EpdFontFamily::REGULAR;
      return GEIST[sizeIdx];
    default:
      // The Helvetica Neue family carries only its Bold cut, in the regular slot.
      style = EpdFontFamily::REGULAR;
      return HELVETICA[sizeIdx];
  }
}

GfxRenderer::Orientation orientationFor(const uint8_t setting) {
  switch (setting) {
    case CrossPointSettings::LANDSCAPE_CW:
      return GfxRenderer::Orientation::LandscapeClockwise;
    case CrossPointSettings::INVERTED:
      return GfxRenderer::Orientation::PortraitInverted;
    case CrossPointSettings::LANDSCAPE_CCW:
      return GfxRenderer::Orientation::LandscapeCounterClockwise;
    default:
      return GfxRenderer::Orientation::Portrait;
  }
}

// "12h 5m" / "45m"
void formatDuration(const uint32_t seconds, char* buf, const size_t bufSize) {
  const unsigned minutes = seconds / 60;
  if (minutes >= 60) {
    snprintf(buf, bufSize, tr(STR_HM_FMT_HOURS), minutes / 60, minutes % 60);
  } else {
    snprintf(buf, bufSize, tr(STR_HM_FMT_MINUTES), minutes);
  }
}

void formatYearText(const uint8_t kind, const CalendarDate& date, char* buf, const size_t bufSize) {
  buf[0] = '\0';
  const unsigned doy = date.dayOfYear();
  const unsigned total = date.daysInYear();
  const unsigned percent = doy * 100u / total;
  switch (kind) {
    case CrossPointSettings::YEAR_TEXT_YEAR:
      snprintf(buf, bufSize, "%u", static_cast<unsigned>(date.year));
      break;
    case CrossPointSettings::YEAR_TEXT_PERCENT:
      snprintf(buf, bufSize, "%u%%", percent);
      break;
    case CrossPointSettings::YEAR_TEXT_DAY_OF_YEAR:
      snprintf(buf, bufSize, tr(STR_YP_FMT_DAY), doy);
      break;
    case CrossPointSettings::YEAR_TEXT_DAYS_LEFT: {
      const unsigned left = total - doy;
      snprintf(buf, bufSize, I18N.get(left == 1 ? StrId::STR_YP_FMT_DAY_LEFT : StrId::STR_YP_FMT_DAYS_LEFT), left);
      break;
    }
    case CrossPointSettings::YEAR_TEXT_FRACTION:
      snprintf(buf, bufSize, "%u / %u", doy, total);
      break;
    case CrossPointSettings::YEAR_TEXT_DATE:
      snprintf(buf, bufSize, "%s %u", I18N.get(MONTH_NAMES[clampIndex(date.month - 1, 12)]),
               static_cast<unsigned>(date.day));
      break;
    case CrossPointSettings::YEAR_TEXT_FRACTION_PERCENT:
      // U+00B7 middle dot, present in every sleep-screen font.
      snprintf(buf, bufSize, "%u / %u \xC2\xB7 %u%%", doy, total, percent);
      break;
    default:
      break;
  }
}

void formatHeatText(const uint8_t kind, const Context& ctx, char* buf, const size_t bufSize) {
  buf[0] = '\0';
  char duration[16];
  switch (kind) {
    case CrossPointSettings::HEAT_TEXT_TOTAL_TIME:
      formatDuration(ctx.summary.totalSeconds, buf, bufSize);
      break;
    case CrossPointSettings::HEAT_TEXT_TODAY_TIME:
      formatDuration(READING_STATS.secondsOn(ctx.lastDay), duration, sizeof(duration));
      snprintf(buf, bufSize, tr(STR_HM_FMT_TODAY), duration);
      break;
    case CrossPointSettings::HEAT_TEXT_STREAK:
      if (ctx.summary.streak == 0) {
        snprintf(buf, bufSize, "%s", tr(STR_HM_NO_STREAK));
      } else if (ctx.summary.streak == 1) {
        snprintf(buf, bufSize, "%s", tr(STR_HM_FMT_STREAK_ONE));
      } else {
        snprintf(buf, bufSize, tr(STR_HM_FMT_STREAK), static_cast<unsigned>(ctx.summary.streak));
      }
      break;
    case CrossPointSettings::HEAT_TEXT_DAYS_READ:
      snprintf(buf, bufSize, tr(STR_HM_FMT_DAYS_READ), static_cast<unsigned>(ctx.summary.daysRead),
               static_cast<unsigned>(ctx.days));
      break;
    case CrossPointSettings::HEAT_TEXT_AVERAGE:
      formatDuration(ctx.days > 0 ? ctx.summary.totalSeconds / ctx.days : 0, duration, sizeof(duration));
      snprintf(buf, bufSize, tr(STR_HM_FMT_AVERAGE), duration);
      break;
    case CrossPointSettings::HEAT_TEXT_DATE:
      formatYearText(CrossPointSettings::YEAR_TEXT_DATE, ctx.date, buf, bufSize);
      break;
    case CrossPointSettings::HEAT_TEXT_YEAR:
      formatYearText(CrossPointSettings::YEAR_TEXT_YEAR, ctx.date, buf, bufSize);
      break;
    default:
      break;
  }
}

// Picks the font for a text line, stepping down through the smaller cuts when
// the chosen size would run past `maxWidth` (e.g. "74 / 90 days" at Large).
void prepareText(TextLine& line, const uint8_t font, const uint8_t size, const int maxWidth,
                 const GfxRenderer& renderer) {
  line.height = 0;
  const uint8_t sizeIdx = clampIndex(size, CrossPointSettings::DOTS_SCALE_COUNT);
  line.fontId = fontIdFor(font, sizeIdx, line.style);
  if (line.text[0] == '\0') return;
  for (int candidate = sizeIdx; candidate > 0; candidate--) {
    if (renderer.getTextWidth(line.fontId, line.text, line.style) <= maxWidth) break;
    line.fontId = fontIdFor(font, static_cast<uint8_t>(candidate - 1), line.style);
  }
  line.height = renderer.getLineHeight(line.fontId);
}

int maxTextWidth(const GfxRenderer& renderer) {
  return renderer.getScreenWidth() -
         2 * MARGIN_PX[clampIndex(SETTINGS.dotsMargin, CrossPointSettings::DOTS_SCALE_COUNT)];
}

// Places the grid and the text block; cols/rows and the two text strings must
// already be filled in.
void placeLayout(Layout& layout, const GfxRenderer& renderer, const uint8_t sizeSetting) {
  const auto& s = SETTINGS;
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int margin = MARGIN_PX[clampIndex(s.dotsMargin, CrossPointSettings::DOTS_SCALE_COUNT)];

  int textHeight = layout.title.height + layout.subtitle.height;
  if (layout.title.height > 0 && layout.subtitle.height > 0) textHeight += TITLE_SUBTITLE_GAP;
  const int textBlock = textHeight > 0 ? textHeight + GRID_TEXT_GAP : 0;

  const int availW = std::max(1, screenW - 2 * margin);
  const int availH = std::max(1, screenH - 2 * margin - textBlock);
  layout.cell = std::max(2, std::min(availW / layout.cols, availH / layout.rows));
  const int scale = DOT_SCALE_PERCENT[clampIndex(sizeSetting, CrossPointSettings::DOTS_SIZE_COUNT)];
  layout.dot = std::clamp(layout.cell * scale / 100, 1, layout.cell);
  layout.gridW = layout.cell * layout.cols;
  layout.gridH = layout.cell * layout.rows;

  const int contentTop = std::max(0, (screenH - (layout.gridH + textBlock)) / 2);
  layout.gridX = (screenW - layout.gridW) / 2;
  int textTop;
  if (s.dotsTextPosition == CrossPointSettings::DOTS_TEXT_TOP) {
    textTop = contentTop;
    layout.gridY = contentTop + textBlock;
  } else {
    layout.gridY = contentTop;
    textTop = contentTop + layout.gridH + GRID_TEXT_GAP;
  }
  layout.title.y = textTop;
  layout.subtitle.y = textTop + (layout.title.height > 0 ? layout.title.height + TITLE_SUBTITLE_GAP : 0);
}

// Without a trustworthy date the subtitle slot carries the fix instead of a number.
void setClockHint(Layout& layout) {
  layout.title.text[0] = '\0';
  snprintf(layout.subtitle.text, sizeof(layout.subtitle.text), "%s", tr(STR_DOTS_CLOCK_NOT_SET));
}

int textX(const Context& ctx, const TextLine& line) {
  const int width = ctx.renderer->getTextWidth(line.fontId, line.text, line.style);
  switch (SETTINGS.dotsTextAlign) {
    case CrossPointSettings::DOTS_ALIGN_LEFT:
      return ctx.layout.gridX;
    case CrossPointSettings::DOTS_ALIGN_RIGHT:
      return ctx.layout.gridX + ctx.layout.gridW - width;
    default:
      return (ctx.renderer->getScreenWidth() - width) / 2;
  }
}

DotClass classFromStyle(const uint8_t style, const bool blackBackground) {
  DotClass c;
  switch (style) {
    case CrossPointSettings::YEAR_DOT_DARK_GRAY:
      c.level = Level::DarkGray;
      break;
    case CrossPointSettings::YEAR_DOT_LIGHT_GRAY:
      c.level = Level::LightGray;
      break;
    case CrossPointSettings::YEAR_DOT_OUTLINE:
      c.outline = true;
      break;
    case CrossPointSettings::YEAR_DOT_HIDDEN:
      c.visible = false;
      break;
    default:  // YEAR_DOT_SOLID: full contrast against the background
      c.level = blackBackground ? Level::White : Level::Black;
      break;
  }
  return c;
}

DotClass todayClass(const uint8_t style, const DotClass& past, const DotClass& future, const bool blackBackground) {
  switch (style) {
    case CrossPointSettings::YEAR_TODAY_AS_PAST:
      return past;
    case CrossPointSettings::YEAR_TODAY_AS_FUTURE:
      return future;
    default:
      // The remaining values mirror YEAR_DOT_STYLE, offset by the two "same as" entries.
      return classFromStyle(style - CrossPointSettings::YEAR_TODAY_SOLID, blackBackground);
  }
}

bool needsGray(const DotClass& c) {
  return c.visible && !c.outline && (c.level == Level::DarkGray || c.level == Level::LightGray);
}

// Heatmap shade for a day's reading time: 1..3 steps away from the background.
DotClass heatClass(const Context& ctx, const uint32_t seconds) {
  if (seconds == 0) return ctx.empty;
  DotClass c;
  int step = 1;
  if (ctx.blackSeconds > 0 && seconds >= ctx.blackSeconds) {
    step = 3;
  } else if (ctx.darkSeconds > 0 && seconds >= ctx.darkSeconds) {
    step = 2;
  }
  // More reading moves further from the background colour on either paper.
  static constexpr Level ON_WHITE[3] = {Level::LightGray, Level::DarkGray, Level::Black};
  static constexpr Level ON_BLACK[3] = {Level::DarkGray, Level::LightGray, Level::White};
  c.level = (ctx.blackBackground ? ON_BLACK : ON_WHITE)[step - 1];
  return c;
}

void fillCircle(const GfxRenderer& renderer, const int x, const int y, const int d, const bool state) {
  if (d <= 2) {
    renderer.fillRect(x, y, d, d, state);
    return;
  }
  const float radius = d / 2.0f;
  for (int row = 0; row < d; row++) {
    const float dy = row + 0.5f - radius;
    const float half = std::sqrt(std::max(0.0f, radius * radius - dy * dy));
    const int x0 = static_cast<int>(std::lround(radius - half));
    const int x1 = static_cast<int>(std::lround(radius + half));
    if (x1 > x0) renderer.fillRect(x + x0, y + row, x1 - x0, 1, state);
  }
}

// state=true paints "ink" (clears bits), false paints "paper" (sets bits) — in
// the grayscale passes, set bits are the pixels the plane updates.
void fillShape(const Context& ctx, const int x, const int y, const int d, const bool state) {
  switch (ctx.shape) {
    case CrossPointSettings::DOTS_SHAPE_SQUARE:
      ctx.renderer->fillRect(x, y, d, d, state);
      break;
    case CrossPointSettings::DOTS_SHAPE_ROUNDED:
      ctx.renderer->fillRoundedRect(x, y, d, d, std::max(1, d / 4), state ? Color::Black : Color::White);
      break;
    default:
      fillCircle(*ctx.renderer, x, y, d, state);
      break;
  }
}

void drawDot(const Context& ctx, const int col, const int row, const DotClass& c, const Pass pass) {
  if (!c.visible) return;
  const Layout& layout = ctx.layout;
  const int x = layout.gridX + col * layout.cell + (layout.cell - layout.dot) / 2;
  const int y = layout.gridY + row * layout.cell + (layout.cell - layout.dot) / 2;
  const bool ink = !ctx.blackBackground;  // pixel state of the contrast colour

  if (c.outline) {
    if (pass != Pass::BW) return;
    fillShape(ctx, x, y, layout.dot, ink);
    const int stroke = std::max(1, layout.dot / 6);
    const int core = layout.dot - 2 * stroke;
    if (core >= 2) fillShape(ctx, x + stroke, y + stroke, core, !ink);
    return;
  }

  switch (pass) {
    case Pass::BW:
      // Grays start out black in the base frame; the planes lighten them.
      fillShape(ctx, x, y, layout.dot, c.level != Level::White);
      break;
    case Pass::GrayLsb:
      if (c.level == Level::DarkGray) fillShape(ctx, x, y, layout.dot, false);
      break;
    case Pass::GrayMsb:
      if (c.level == Level::DarkGray || c.level == Level::LightGray) fillShape(ctx, x, y, layout.dot, false);
      break;
  }
}

void drawTextLines(const Context& ctx, const Pass pass) {
  const bool ink = !ctx.blackBackground;
  for (const TextLine* line : {&ctx.layout.title, &ctx.layout.subtitle}) {
    if (line->height == 0) continue;
    const int x = textX(ctx, *line);
    if (pass == Pass::BW) {
      ctx.renderer->drawText(line->fontId, x, line->y, line->text, ink, line->style);
    } else if (!ctx.blackBackground) {
      // Anti-aliasing planes. The 2-bit font grays are defined against paper,
      // so white-on-black text stays crisp 1-bit instead of getting a dark halo.
      ctx.renderer->drawText(line->fontId, x, line->y, line->text, true, line->style);
    }
  }
}

bool textNeedsGray(const Context& ctx) {
  return !ctx.blackBackground && (ctx.layout.title.height > 0 || ctx.layout.subtitle.height > 0);
}

// ---- Year Progress ---------------------------------------------------------

void drawYear(const Context& ctx, const Pass pass) {
  const uint16_t calendarYear = ctx.date.valid ? ctx.date.year : FALLBACK_YEAR;
  int month = 0;  // 0-based, for the month layout
  int monthStart = 0;
  int monthDays = CivilDate::daysInMonth(calendarYear, 1);
  for (int i = 0; i < ctx.totalDays; i++) {
    int row, col;
    if (ctx.byMonth) {
      while (i >= monthStart + monthDays && month < 11) {
        monthStart += monthDays;
        month++;
        monthDays = CivilDate::daysInMonth(calendarYear, static_cast<uint8_t>(month + 1));
      }
      row = month;
      col = i - monthStart;
    } else {
      row = i / ctx.layout.cols;
      col = i % ctx.layout.cols;
    }
    const DotClass& c = (ctx.todayIndex < 0 || i > ctx.todayIndex) ? ctx.future
                        : (i == ctx.todayIndex)                    ? ctx.today
                                                                   : ctx.past;
    drawDot(ctx, col, row, c, pass);
  }
  drawTextLines(ctx, pass);
}

bool prepareYear(Context& ctx) {
  const auto& s = SETTINGS;
  ctx.shape = s.yearDotShape;
  ctx.past = classFromStyle(s.yearPastStyle, ctx.blackBackground);
  ctx.future = classFromStyle(s.yearFutureStyle, ctx.blackBackground);
  ctx.today = todayClass(s.yearTodayStyle, ctx.past, ctx.future, ctx.blackBackground);
  ctx.totalDays = ctx.date.valid ? ctx.date.daysInYear() : 365;
  ctx.todayIndex = ctx.date.valid ? ctx.date.dayOfYear() - 1 : -1;
  ctx.byMonth = s.yearLayout == CrossPointSettings::YEAR_LAYOUT_MONTHS;

  Layout& layout = ctx.layout;
  if (ctx.byMonth) {
    layout.cols = MONTH_COLUMNS;
    layout.rows = 12;
  } else {
    layout.cols =
        std::clamp<int>(s.yearColumns, CrossPointSettings::DOTS_COLUMNS_MIN, CrossPointSettings::DOTS_COLUMNS_MAX);
    layout.rows = (ctx.totalDays + layout.cols - 1) / layout.cols;
  }

  if (ctx.date.valid) {
    formatYearText(s.yearTitle, ctx.date, layout.title.text, sizeof(layout.title.text));
    formatYearText(s.yearSubtitle, ctx.date, layout.subtitle.text, sizeof(layout.subtitle.text));
  } else {
    setClockHint(layout);
  }
  prepareText(layout.title, s.dotsTitleFont, s.dotsTitleSize, maxTextWidth(*ctx.renderer), *ctx.renderer);
  prepareText(layout.subtitle, s.dotsSubtitleFont, ctx.date.valid ? s.dotsSubtitleSize : 0, maxTextWidth(*ctx.renderer),
              *ctx.renderer);
  placeLayout(layout, *ctx.renderer, s.yearDotSize);

  const int lastIndex = ctx.totalDays - 1;
  return (ctx.todayIndex > 0 && needsGray(ctx.past)) || (ctx.todayIndex >= 0 && needsGray(ctx.today)) ||
         (ctx.todayIndex < lastIndex && needsGray(ctx.future)) || textNeedsGray(ctx);
}

// ---- Reading Heatmap -------------------------------------------------------

void drawHeat(const Context& ctx, const Pass pass) {
  for (int i = 0; i < ctx.days; i++) {
    const int slot = ctx.firstOffset + i;
    const int row = slot / ctx.layout.cols;
    const int col = slot % ctx.layout.cols;
    const uint32_t seconds = ctx.date.valid ? READING_STATS.secondsOn(ctx.firstDay + i) : 0;
    drawDot(ctx, col, row, heatClass(ctx, seconds), pass);
  }
  drawTextLines(ctx, pass);
}

bool prepareHeat(Context& ctx) {
  const auto& s = SETTINGS;
  ctx.shape = s.heatShape;
  ctx.days = std::clamp<int>(s.heatDays, CrossPointSettings::HEAT_DAYS_MIN, CrossPointSettings::HEAT_DAYS_MAX);
  ctx.lastDay = ctx.date.valid ? ctx.date.epochDay() : 0;
  ctx.firstDay = ctx.lastDay - ctx.days + 1;
  ctx.byWeek = s.heatLayout == CrossPointSettings::HEAT_LAYOUT_WEEKS;
  ctx.empty =
      classFromStyle(s.heatEmptyStyle == CrossPointSettings::HEAT_EMPTY_HIDDEN ? CrossPointSettings::YEAR_DOT_HIDDEN
                                                                               : CrossPointSettings::YEAR_DOT_OUTLINE,
                     ctx.blackBackground);
  if (ctx.date.valid) ctx.summary = READING_STATS.summarize(ctx.firstDay, ctx.lastDay);

  const uint8_t scale = clampIndex(s.heatScale, CrossPointSettings::HEAT_SCALE_COUNT);
  if (scale == CrossPointSettings::HEAT_SCALE_AUTO) {
    // Relative to the busiest day in the window, like GitHub's quartiles.
    ctx.darkSeconds = ctx.summary.maxSeconds / 3 + 1;
    ctx.blackSeconds = ctx.summary.maxSeconds * 2 / 3 + 1;
  } else {
    ctx.darkSeconds = HEAT_THRESHOLDS[scale][0] * 60u;
    ctx.blackSeconds = HEAT_THRESHOLDS[scale][1] * 60u;
  }

  Layout& layout = ctx.layout;
  if (ctx.byWeek) {
    layout.cols = WEEK_COLUMNS;
    // Align the first day to its weekday column so rows are calendar weeks.
    const uint8_t weekday = CivilDate::weekday(ctx.firstDay);  // 0 = Sunday
    ctx.firstOffset = s.heatWeekStart == CrossPointSettings::WEEK_START_SUNDAY ? weekday : (weekday + 6) % 7;
  } else {
    layout.cols =
        std::clamp<int>(s.heatColumns, CrossPointSettings::DOTS_COLUMNS_MIN, CrossPointSettings::DOTS_COLUMNS_MAX);
    ctx.firstOffset = 0;
  }
  layout.rows = (ctx.firstOffset + ctx.days + layout.cols - 1) / layout.cols;

  if (ctx.date.valid) {
    formatHeatText(s.heatTitle, ctx, layout.title.text, sizeof(layout.title.text));
    formatHeatText(s.heatSubtitle, ctx, layout.subtitle.text, sizeof(layout.subtitle.text));
  } else {
    setClockHint(layout);
  }
  prepareText(layout.title, s.dotsTitleFont, s.dotsTitleSize, maxTextWidth(*ctx.renderer), *ctx.renderer);
  prepareText(layout.subtitle, s.dotsSubtitleFont, ctx.date.valid ? s.dotsSubtitleSize : 0, maxTextWidth(*ctx.renderer),
              *ctx.renderer);
  placeLayout(layout, *ctx.renderer, s.heatDotSize);

  // Any reading at all produces a gray shade on either background.
  return ctx.summary.maxSeconds > 0 || textNeedsGray(ctx);
}

// ---- Presentation ----------------------------------------------------------

void present(GfxRenderer& renderer, const Context& ctx, const DrawFn draw, const bool grayscale) {
  renderer.setRenderMode(GfxRenderer::BW);
  renderer.clearScreen(ctx.blackBackground ? 0x00 : 0xFF);
  draw(ctx, Pass::BW);

  if (!grayscale) {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    return;
  }

  // Same sequence as the bitmap sleep screens: HALF base (the gray LUT is
  // calibrated against it), then the two planes.
  renderer.displayGrayscaleBase(HalDisplay::HALF_REFRESH);

  renderer.clearScreen(0x00);
  renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
  draw(ctx, Pass::GrayLsb);
  renderer.copyGrayscaleLsbBuffers();

  renderer.clearScreen(0x00);
  renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
  draw(ctx, Pass::GrayMsb);
  renderer.copyGrayscaleMsbBuffers();

  renderer.displayGrayBuffer();
  renderer.setRenderMode(GfxRenderer::BW);
}

}  // namespace

CalendarDate CalendarDate::today() {
  CalendarDate date;
  date.valid = halClock.getLocalDate(date.year, date.month, date.day, SETTINGS.clockUtcOffsetQ);
  if (!date.valid) LOG_INF("DOTS", "No usable RTC date");
  return date;
}

uint16_t CalendarDate::daysInYear() const { return CivilDate::isLeapYear(year) ? 366 : 365; }

uint16_t CalendarDate::dayOfYear() const {
  if (!valid) return 0;
  uint16_t ordinal = day;
  for (uint8_t m = 1; m < month; m++) ordinal += CivilDate::daysInMonth(year, m);
  return ordinal;
}

int32_t CalendarDate::epochDay() const { return valid ? CivilDate::daysFromCivil(year, month, day) : 0; }

void DotsScreen::render(GfxRenderer& renderer, const Kind kind, const CalendarDate& date) {
  const auto savedOrientation = renderer.getOrientation();
  renderer.setOrientation(orientationFor(SETTINGS.dotsOrientation));

  Context ctx;
  ctx.renderer = &renderer;
  ctx.date = date;
  ctx.blackBackground = SETTINGS.dotsBackground == CrossPointSettings::DOTS_BG_BLACK;

  const bool year = kind == Kind::YearProgress;
  const bool grayscale = year ? prepareYear(ctx) : prepareHeat(ctx);
  LOG_DBG("DOTS", "%s date=%u-%02u-%02u valid=%d grid=%dx%d cell=%d dot=%d gray=%d", year ? "year" : "heat", date.year,
          date.month, date.day, date.valid, ctx.layout.cols, ctx.layout.rows, ctx.layout.cell, ctx.layout.dot,
          grayscale);

  present(renderer, ctx, year ? drawYear : drawHeat, grayscale);
  renderer.setOrientation(savedOrientation);
}
