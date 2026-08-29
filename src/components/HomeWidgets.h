#pragma once

#include <GfxRenderer.h>

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
// Band height for the full (icon + value + caption) or compact (icon + value) layout.
int bandHeight(const GfxRenderer& renderer, bool compact);
void draw(const GfxRenderer& renderer, const Rect& band, bool compact);
}  // namespace HomeWidgets
