#pragma once

#include <GfxRenderer.h>

#include <cstdint>

// Local calendar date from the RTC, as the dot screens see it.
struct CalendarDate {
  uint16_t year = 0;
  uint8_t month = 0;  // 1-12
  uint8_t day = 0;    // 1-31
  bool valid = false;

  // Local date with the status-bar clock's UTC offset applied. `valid` stays
  // false when the device has no RTC or the clock was never set.
  static CalendarDate today();

  uint16_t daysInYear() const;
  uint16_t dayOfYear() const;  // 1-based; 0 when invalid
  int32_t epochDay() const;    // days since 1970-01-01; 0 when invalid
};

// The two "dot grid" sleep screens: Year Progress (one dot per day of the
// year, elapsed days muted) and Reading Heatmap (one square per recent day,
// shaded by reading time). Both share the SETTINGS.dots* styling — fonts, text
// placement, margins, background, orientation — and are edited in
// DotsScreenSettingsActivity. Used by the sleep screen and its preview.
namespace DotsScreen {
enum class Kind : uint8_t { YearProgress, ReadingHeatmap };

// Paints the screen and pushes it to the panel: a single HALF refresh when
// only black and white are used, otherwise the same BW-base + LSB/MSB-plane
// pipeline the bitmap sleep screens use. The framebuffer is scratch afterwards,
// so the caller must repaint before its next BW refresh.
void render(GfxRenderer& renderer, Kind kind, const CalendarDate& date);
}  // namespace DotsScreen
