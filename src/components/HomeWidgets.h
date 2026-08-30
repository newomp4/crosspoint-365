#pragma once

#include <GfxRenderer.h>

#include <cstddef>

struct Rect;

// The home screen's row of complications: up to four slots, each an icon, a
// value and a caption (clock, date, reading time, streak, weather, ...),
// configured in SETTINGS.homeWidget1..4 and edited in
// HomeWidgetsSettingsActivity. Draws straight into the current framebuffer
// with no heap allocation, so it costs the home screen nothing but pixels.
namespace HomeWidgets {
// Configured (non-empty) slots, 0..4. The band is hidden when this is 0.
int slotCount();
// A clock or date widget is configured, so the home screen repaints when the minute changes.
bool showsClock();
bool showsWeather();
// Band height for the full (icon + value + caption) or compact (icon + value)
// layout. Themes with homeWidgetTiles get cards two to a row instead of a row.
int bandHeight(const GfxRenderer& renderer, bool compact);
void draw(const GfxRenderer& renderer, const Rect& band, bool compact);
// "Saturday, Aug 29" for the Mono header; false when the clock is not set.
bool formatHeaderDate(char* buf, size_t bufSize);
// Last-14-days reading bars on a widget-style card; fills the flexible space
// under the continue-reading card on tile themes.
void drawActivityPanel(const GfxRenderer& renderer, const Rect& rect);
// Upcoming calendar events on the same card style, for a home section slot.
void drawCalendarPanel(const GfxRenderer& renderer, const Rect& rect);
}  // namespace HomeWidgets
