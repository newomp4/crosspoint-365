#pragma once

#include <cstdint>

// Per-day reading time. Counts wall-clock time spent in a reader activity, but
// only while the reader is being used: the gap between two inputs (page turns,
// taps, tilts) counts up to MAX_INPUT_GAP_MS, so a book left open on the desk
// stops accruing after that. Days are keyed by the RTC's local calendar date;
// without a usable clock nothing is recorded. Persisted as a tiny CSV on the
// SD card and read back by the Reading Heatmap sleep screen.
class ReadingStats {
 public:
  static ReadingStats& getInstance();

  // Reader-activity lifetime + the user inputs that happen inside it.
  void beginSession(unsigned long nowMs);
  void noteInput(unsigned long nowMs);
  void endSession(unsigned long nowMs);
  // Call every main-loop pass while reading: credits an idle-capped gap once
  // and flushes dirty data to the SD card at a low cadence.
  void tick(unsigned long nowMs);

  // Seconds read on an epoch day (days since 1970-01-01). Loads on first use.
  uint32_t secondsOn(int32_t epochDay);

  struct Summary {
    uint32_t totalSeconds = 0;
    uint32_t maxSeconds = 0;
    uint16_t daysRead = 0;
    // Consecutive days with reading ending on lastDay (or the day before it,
    // so today's not-yet-started reading doesn't zero the streak).
    uint16_t streak = 0;
  };
  Summary summarize(int32_t firstDay, int32_t lastDay);

  bool load();
  bool save();

 private:
  ReadingStats() = default;

  static constexpr uint16_t MAX_DAYS = 370;
  static constexpr unsigned long MAX_INPUT_GAP_MS = 5UL * 60UL * 1000UL;
  // Flush cadence while reading. Anything unsaved is also written when the
  // reader closes or the device sleeps, so a longer interval only risks the
  // last few minutes on a crash — and spares the SD card a write every few
  // pages.
  static constexpr unsigned long SAVE_INTERVAL_MS = 10UL * 60UL * 1000UL;
  // The RTC is on I2C; the date is re-read this often. A day boundary landing
  // a few minutes late only moves that sliver of time to the next day.
  static constexpr unsigned long DAY_CACHE_MS = 5UL * 60UL * 1000UL;
  // Days are stored relative to 2024-01-01 so an entry fits in 4 bytes.
  static constexpr int32_t DAY_BASE = 19723;

  struct Entry {
    uint16_t day;      // epoch day - DAY_BASE
    uint16_t seconds;  // saturates at 65535 (18 h)
  };
  Entry entries[MAX_DAYS] = {};  // sorted by day
  uint16_t count = 0;

  bool loaded = false;
  bool dirty = false;
  bool inSession = false;
  bool idleCredited = false;
  unsigned long lastInputMs = 0;
  unsigned long lastSaveMs = 0;
  uint32_t pendingMs = 0;

  int32_t cachedDay = -1;  // epoch day, cached for DAY_CACHE_MS
  unsigned long cachedDayAtMs = 0;
  bool dayCacheValid = false;

  int32_t currentDay(unsigned long nowMs);
  void credit(unsigned long nowMs, unsigned long elapsedMs);
  void addSeconds(int32_t epochDay, uint32_t seconds);
  int findIndex(uint16_t relDay) const;
};

#define READING_STATS ReadingStats::getInstance()
