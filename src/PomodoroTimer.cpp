#include "PomodoroTimer.h"

#include <Arduino.h>
#include <Logging.h>

void PomodoroTimer::start(const uint16_t focusMinutes, const uint16_t breakMinutes) {
  focusMin = focusMinutes;
  breakMin = breakMinutes;
  sessions = 0;
  endedPhase = Phase::Idle;
  paused = false;
  lastActivityMs = millis();
  enterPhase(Phase::Focus);
  LOG_INF("POMO", "Started: %u min focus / %u min break", focusMinutes, breakMinutes);
}

void PomodoroTimer::stop() {
  currentPhase = Phase::Idle;
  endedPhase = Phase::Idle;
  paused = false;
}

void PomodoroTimer::togglePause() {
  if (currentPhase == Phase::Idle) return;
  if (paused) {
    phaseStartMs = millis();
    phaseDurationMs = pausedRemainingMs;
    paused = false;
  } else {
    pausedRemainingMs = remainingSeconds() * 1000;
    paused = true;
  }
}

uint32_t PomodoroTimer::remainingSeconds() const {
  if (currentPhase == Phase::Idle) return 0;
  if (paused) return pausedRemainingMs / 1000;
  const uint32_t elapsed = millis() - phaseStartMs;
  return elapsed >= phaseDurationMs ? 0 : (phaseDurationMs - elapsed + 999) / 1000;
}

void PomodoroTimer::update() {
  if (currentPhase == Phase::Idle || paused) return;
  if (millis() - phaseStartMs < phaseDurationMs) return;
  endedPhase = currentPhase;
  if (currentPhase == Phase::Focus) {
    sessions++;
    enterPhase(Phase::Break);
    return;
  }
  // Break over. If nothing was pressed for an entire focus+break cycle the
  // session is abandoned: stop rather than cycle (and hold off auto-sleep)
  // unattended forever.
  const uint32_t cycleMs = (static_cast<uint32_t>(focusMin) + breakMin) * 60000UL;
  if (millis() - lastActivityMs > cycleMs) {
    LOG_INF("POMO", "No input for a full cycle; stopping abandoned session");
    stop();
    return;
  }
  enterPhase(Phase::Focus);
}

PomodoroTimer::Phase PomodoroTimer::consumePhaseEnd() {
  const Phase p = endedPhase;
  endedPhase = Phase::Idle;
  return p;
}

void PomodoroTimer::enterPhase(const Phase next) {
  currentPhase = next;
  phaseStartMs = millis();
  phaseDurationMs = (next == Phase::Focus ? focusMin : breakMin) * 60000UL;
}
