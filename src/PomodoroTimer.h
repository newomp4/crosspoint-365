#pragma once
#include <cstdint>

// Global focus/break timer. Lives across activities so a session started in
// the Focus Timer screen keeps running while reading; the reader's status bar
// shows the remaining time. millis()-based — a deep sleep ends the session
// (the main loop keeps the device awake while one is running).
class PomodoroTimer {
 public:
  enum class Phase : uint8_t { Idle, Focus, Break };

  static PomodoroTimer& getInstance() {
    static PomodoroTimer instance;
    return instance;
  }

  void start(uint16_t focusMinutes, uint16_t breakMinutes);
  void stop();
  void togglePause();

  // Advances phases when their time is up. Call from any loop; cheap.
  void update();

  Phase phase() const { return currentPhase; }
  bool isRunning() const { return currentPhase != Phase::Idle && !paused; }
  bool isActive() const { return currentPhase != Phase::Idle; }
  bool isPaused() const { return paused; }
  uint32_t remainingSeconds() const;
  uint32_t phaseSeconds() const { return phaseDurationMs / 1000; }
  uint16_t sessionsDone() const { return sessions; }

  // One-shot: the phase that just completed (Idle when none). Consumed by the
  // foreground activity to announce the switch.
  Phase consumePhaseEnd();

 private:
  PomodoroTimer() = default;

  Phase currentPhase = Phase::Idle;
  Phase endedPhase = Phase::Idle;
  bool paused = false;
  uint32_t phaseStartMs = 0;
  uint32_t phaseDurationMs = 0;
  uint32_t pausedRemainingMs = 0;
  uint16_t focusMin = 25;
  uint16_t breakMin = 5;
  uint16_t sessions = 0;

  void enterPhase(Phase next);
};

#define POMODORO PomodoroTimer::getInstance()
