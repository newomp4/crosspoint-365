#include "SleepScreenPickerActivity.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/CalendarScreen.h"
#include "components/DotsScreen.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/Logo120.h"
#include "util/ButtonNavigator.h"

namespace {
using S = CrossPointSettings;
constexpr int MODE_COUNT = S::SLEEP_SCREEN_MODE_COUNT;

constexpr StrId MODE_NAMES[MODE_COUNT] = {
    StrId::STR_DARK,         StrId::STR_LIGHT,        StrId::STR_CUSTOM,        StrId::STR_COVER,
    StrId::STR_COVER_CUSTOM, StrId::STR_NONE_OPT,     StrId::STR_QUICK_RESUME,  StrId::STR_TRANSPARENT,
    StrId::STR_YEAR_PROGRESS, StrId::STR_READING_HEATMAP, StrId::STR_CALENDAR,
};
}  // namespace

void SleepScreenPickerActivity::onEnter() {
  Activity::onEnter();
  index = SETTINGS.sleepScreen < MODE_COUNT ? SETTINGS.sleepScreen : 0;
  requestUpdate();
}

void SleepScreenPickerActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    SETTINGS.sleepScreen = static_cast<uint8_t>(index);
    SETTINGS.saveToFile();
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right) ||
      mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    index = ButtonNavigator::nextIndex(index, MODE_COUNT);
    requestUpdate();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Left) || mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    index = ButtonNavigator::previousIndex(index, MODE_COUNT);
    requestUpdate();
  }
}

// Bottom-center capsule naming the shown mode plus the choose hint. Drawn last
// so it survives screens that paint or invert the full frame.
void SleepScreenPickerActivity::drawChip(const bool onDark) {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  char label[64];
  snprintf(label, sizeof(label), "\x3C  %s  \x3E", I18N.get(MODE_NAMES[index]));
  char sub[48];
  snprintf(sub, sizeof(sub), "%s \xC2\xB7 %d/%d", tr(STR_SLEEP_PICK_HINT), index + 1, MODE_COUNT);

  const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, label);
  const int subWidth = renderer.getTextWidth(SMALL_FONT_ID, sub);
  const int chipWidth = std::max(labelWidth, subWidth) + 36;
  const int labelLine = renderer.getLineHeight(UI_10_FONT_ID);
  const int subLine = renderer.getLineHeight(SMALL_FONT_ID);
  const int chipHeight = 10 + labelLine + 2 + subLine + 10;
  const int x = (pageWidth - chipWidth) / 2;
  const int y = pageHeight - chipHeight - 22;

  renderer.fillRoundedRect(x, y, chipWidth, chipHeight, 14, onDark ? Color::White : Color::Black);
  renderer.fillRoundedRect(x + 2, y + 2, chipWidth - 4, chipHeight - 4, 12, onDark ? Color::Black : Color::White);
  renderer.drawText(UI_10_FONT_ID, x + (chipWidth - labelWidth) / 2, y + 10, label, !onDark, EpdFontFamily::BOLD);
  renderer.drawText(SMALL_FONT_ID, x + (chipWidth - subWidth) / 2, y + 10 + labelLine + 2, sub, !onDark);
}

void SleepScreenPickerActivity::drawPlaceholder(const StrId title, const StrId description) {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();
  const int cardWidth = pageWidth - 80;
  const int cardHeight = 200;
  const int x = 40;
  const int y = (pageHeight - cardHeight) / 2 - 40;
  renderer.fillRoundedRect(x, y, cardWidth, cardHeight, 16, Color::LightGray);
  renderer.drawCenteredText(UI_12_FONT_ID, y + 56, I18N.get(title), true, EpdFontFamily::BOLD);
  const auto lines = renderer.wrappedText(UI_10_FONT_ID, I18N.get(description), cardWidth - 48, 3);
  int textY = y + 56 + renderer.getLineHeight(UI_12_FONT_ID) + 12;
  for (const auto& line : lines) {
    const int w = renderer.getTextWidth(UI_10_FONT_ID, line.c_str());
    renderer.drawText(UI_10_FONT_ID, (pageWidth - w) / 2, textY, line.c_str(), true);
    textY += renderer.getLineHeight(UI_10_FONT_ID);
  }
}

void SleepScreenPickerActivity::render(RenderLock&&) {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  bool selfDisplayed = false;
  bool onDark = false;

  switch (index) {
    case S::DARK:
    case S::LIGHT: {
      renderer.clearScreen();
      renderer.drawImage(Logo120, (pageWidth - 120) / 2, (pageHeight - 120) / 2 - 30, 120, 120);
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 40, tr(STR_CROSSPOINT), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 65, tr(STR_SLEEPING));
      if (index == S::DARK) {
        renderer.invertScreen();
        onDark = true;
      }
      break;
    }
    case S::BLANK:
      renderer.clearScreen();
      break;
    case S::YEAR_PROGRESS:
      DotsScreen::render(renderer, DotsScreen::Kind::YearProgress, CalendarDate::today());
      selfDisplayed = true;
      onDark = SETTINGS.dotsBackground == S::DOTS_BG_BLACK;
      break;
    case S::READING_HEATMAP:
      DotsScreen::render(renderer, DotsScreen::Kind::ReadingHeatmap, CalendarDate::today());
      selfDisplayed = true;
      onDark = SETTINGS.dotsBackground == S::DOTS_BG_BLACK;
      break;
    case S::CALENDAR_VIEW:
      CalendarScreen::render(renderer);
      selfDisplayed = true;
      break;
    case S::CUSTOM:
      drawPlaceholder(StrId::STR_CUSTOM, StrId::STR_SLEEP_PREV_FILE);
      break;
    case S::COVER:
      drawPlaceholder(StrId::STR_COVER, StrId::STR_SLEEP_PREV_COVER);
      break;
    case S::COVER_CUSTOM:
      drawPlaceholder(StrId::STR_COVER_CUSTOM, StrId::STR_SLEEP_PREV_COVER_CUSTOM);
      break;
    case S::QUICK_RESUME:
      drawPlaceholder(StrId::STR_QUICK_RESUME, StrId::STR_SLEEP_PREV_QR);
      break;
    case S::TRANSPARENT_CUSTOM:
    default:
      drawPlaceholder(StrId::STR_TRANSPARENT, StrId::STR_SLEEP_PREV_TRANSPARENT);
      break;
  }

  drawChip(onDark);
  // Self-displaying previews already ran their full-quality pass; the chip
  // lands with a quick partial. Everything else is one paint.
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  (void)selfDisplayed;
}
