#include "PomodoroActivity.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "PomodoroTimer.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Cycled by Left/Right (focus) and Up/Down (break) while idle.
constexpr uint8_t FOCUS_PRESETS[] = {15, 20, 25, 30, 45, 60, 90};
constexpr uint8_t BREAK_PRESETS[] = {3, 5, 10, 15, 20};

template <size_t N>
uint8_t cyclePreset(const uint8_t (&presets)[N], const uint8_t current, const int step) {
  int nearest = 0;
  for (size_t i = 0; i < N; i++) {
    if (presets[i] <= current) nearest = static_cast<int>(i);
  }
  nearest = (nearest + step + static_cast<int>(N)) % static_cast<int>(N);
  return presets[nearest];
}
}  // namespace

void PomodoroActivity::onEnter() {
  Activity::onEnter();
  POMODORO.consumePhaseEnd();  // a switch that happened off-screen needs no flash
  lastShownMinute = 0xFFFFFFFF;
  requestUpdate();
}

void PomodoroActivity::adjustFocus(const int step) {
  SETTINGS.pomodoroFocusMin = cyclePreset(FOCUS_PRESETS, SETTINGS.pomodoroFocusMin, step);
  SETTINGS.saveToFile();
  requestUpdate();
}

void PomodoroActivity::adjustBreak(const int step) {
  SETTINGS.pomodoroBreakMin = cyclePreset(BREAK_PRESETS, SETTINGS.pomodoroBreakMin, step);
  SETTINGS.saveToFile();
  requestUpdate();
}

void PomodoroActivity::loop() {
  POMODORO.update();
  if (POMODORO.consumePhaseEnd() != PomodoroTimer::Phase::Idle) {
    announcePhase = true;
    requestUpdate();
  } else if (POMODORO.isActive()) {
    const uint32_t minute = POMODORO.remainingSeconds() / 60;
    if (minute != lastShownMinute) requestUpdate();
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (POMODORO.isActive()) {
      POMODORO.togglePause();
    } else {
      POMODORO.start(SETTINGS.pomodoroFocusMin, SETTINGS.pomodoroBreakMin);
    }
    requestUpdate();
    return;
  }
  if (!POMODORO.isActive()) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) adjustFocus(-1);
    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) adjustFocus(1);
    if (mappedInput.wasPressed(MappedInputManager::Button::Up)) adjustBreak(1);
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) adjustBreak(-1);
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    POMODORO.stop();
    requestUpdate();
  }
}

void PomodoroActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_POMODORO));

  const bool active = POMODORO.isActive();
  const auto phase = POMODORO.phase();
  char buf[40];

  // Phase label above the big time.
  const char* phaseLabel = !active                                ? tr(STR_POMO_FOCUS)
                           : phase == PomodoroTimer::Phase::Focus ? tr(STR_POMO_FOCUS)
                                                                  : tr(STR_POMO_BREAK);
  const int centerY = pageHeight * 2 / 5;
  const int timeLineHeight = renderer.getLineHeight(GEIST_40_FONT_ID);
  renderer.drawCenteredText(UI_10_FONT_ID, centerY - timeLineHeight - 26, phaseLabel, true, EpdFontFamily::BOLD);

  // Big countdown (running) or the configured focus length (idle).
  uint32_t shownSeconds;
  if (active) {
    shownSeconds = POMODORO.remainingSeconds();
  } else {
    shownSeconds = SETTINGS.pomodoroFocusMin * 60u;
  }
  lastShownMinute = shownSeconds / 60;
  snprintf(buf, sizeof(buf), "%lu:%02lu", static_cast<unsigned long>(shownSeconds / 60),
           static_cast<unsigned long>(shownSeconds % 60));
  renderer.drawCenteredText(GEIST_40_FONT_ID, centerY - timeLineHeight, buf, true, EpdFontFamily::BOLD);

  // Progress track, filled by elapsed share of the phase.
  const int barWidth = std::min(pageWidth - 2 * metrics.contentSidePadding, 320);
  const int barX = (pageWidth - barWidth) / 2;
  const int barY = centerY + 24;
  renderer.fillRoundedRect(barX, barY, barWidth, 8, 4, Color::LightGray);
  if (active && POMODORO.phaseSeconds() > 0) {
    const uint32_t total = POMODORO.phaseSeconds();
    const int fill = static_cast<int>(static_cast<uint64_t>(barWidth) * (total - shownSeconds) / total);
    if (fill > 0) renderer.fillRoundedRect(barX, barY, std::max(fill, 8), 8, 4, Color::Black);
  }

  int y = barY + 32;
  if (active) {
    if (POMODORO.isPaused()) {
      renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_POMO_PAUSED), true, EpdFontFamily::BOLD);
      y += renderer.getLineHeight(UI_10_FONT_ID) + 6;
    }
    if (POMODORO.sessionsDone() > 0) {
      snprintf(buf, sizeof(buf), tr(STR_POMO_SESSIONS_FMT), static_cast<unsigned>(POMODORO.sessionsDone()));
      renderer.drawCenteredText(UI_10_FONT_ID, y, buf);
    }
  } else {
    // Idle: both lengths plus how to change them.
    char focusText[16], breakText[16];
    snprintf(focusText, sizeof(focusText), tr(STR_POMO_MIN_FMT), static_cast<unsigned>(SETTINGS.pomodoroFocusMin));
    snprintf(breakText, sizeof(breakText), tr(STR_POMO_MIN_FMT), static_cast<unsigned>(SETTINGS.pomodoroBreakMin));
    snprintf(buf, sizeof(buf), "%s %s \xC2\xB7 %s %s", tr(STR_POMO_FOCUS), focusText, tr(STR_POMO_BREAK), breakText);
    renderer.drawCenteredText(UI_10_FONT_ID, y, buf);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 6;
    renderer.drawCenteredText(SMALL_FONT_ID, y, tr(STR_POMO_ADJUST_HINT));
  }

  const char* confirmLabel = !active               ? tr(STR_POMO_START)
                             : POMODORO.isPaused() ? tr(STR_POMO_RESUME)
                                                   : tr(STR_POMO_PAUSE);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, active ? tr(STR_POMO_STOP) : "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (announcePhase) {
    // Phase switch: flash inverted once so the change is visible from afar,
    // then settle on a clean full paint.
    announcePhase = false;
    renderer.invertScreen();
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    delay(220);
    renderer.invertScreen();
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    return;
  }
  renderer.displayBuffer();
}
