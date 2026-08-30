#pragma once

#include <GfxRenderer.h>

// Calendar sleep screen: today's date as a headline, then the upcoming events
// from CalendarStore grouped by day, in the UI fonts (black and white, one
// HALF refresh). Draws straight to the panel.
namespace CalendarScreen {
void render(GfxRenderer& renderer);
}
