#pragma once

#include <cstddef>
#include <cstdint>

// Reading time, three ways: per calendar day, per book, and all-time. Counts
// wall-clock time spent in a reader activity, but only while the reader is
// being used: the gap between two inputs (page turns, taps, tilts) counts up
// to MAX_INPUT_GAP_MS, so a book left open on the desk stops accruing after
// that. Days are keyed by the RTC's local calendar date; without a usable
// clock nothing is recorded. Persisted as a tiny CSV on the SD card; read by
// the Reading Heatmap sleep screen and the home-screen widgets.
class ReadingStats {
 public:
  static ReadingStats& getInstance();

  // Reader-activity lifetime + the user inputs that happen inside it.
  // bookPath attributes the session's time to that book (may be null).
  void beginSession(unsigned long nowMs, const char* bookPath);
  void noteInput(unsigned long nowMs);
  void endSession(unsigned long nowMs);
  // Call every main-loop pass while reading: credits an idle-capped gap once
  // and flushes dirty data to the SD card at a low cadence.
  void tick(unsigned long nowMs);

  // Seconds read on an epoch day (days since 1970-01-01). Loads on first use.
  uint32_t secondsOn(int32_t epochDay);
  uint32_t allTimeSeconds();
  uint32_t secondsForBook(const char* bookPath);

  struct Summary {
    uint32_t totalSeconds = 0;
    uint32_t maxSeconds = 0;
    uint16_t daysRead = 0;
    // Consecutive days with reading ending on lastDay (or the day before it,
    // so today's not-yet-started reading doesn't zero the streak).
    uint16_t streak = 0;
  };
  Summary summarize(int32_t firstDay, int32_t lastDay);

  // "12h 5m" / "45m" / "128h" (minutes dropped from three-digit hour counts).
  static void formatDuration(uint32_t seconds, char* buf, size_t bufSize);
  // FNV-1a over the path: stable across builds, unlike std::hash.
  static uint32_t bookHash(const char* path);

  void ensureLoaded() {
    if (!loaded) load();
  }
  bool load();
  bool save();

 private:
  ReadingStats() = default;

  static constexpr uint16_t MAX_DAYS = 370;
  static constexpr uint8_t MAX_BOOKS = 48;
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

  struct DayEntry {
    uint16_t day;      // epoch day - DAY_BASE
    uint16_t seconds;  // saturates at 65535 (18 h)
  };
  struct BookEntry {
    uint32_t hash;
    uint32_t seconds;
    uint16_t lastDay;  // epoch day - DAY_BASE of the last credit; eviction order
  };
  DayEntry days[MAX_DAYS] = {};  // sorted by day
  uint16_t count = 0;
  BookEntry books[MAX_BOOKS] = {};
  uint8_t bookCount = 0;
  uint32_t allTime = 0;

  bool loaded = false;
  bool dirty = false;
  bool inSession = false;
  bool idleCredited = false;
  uint32_t sessionBook = 0;  // bookHash of the open book, 0 = none
  unsigned long lastInputMs = 0;
  unsigned long lastSaveMs = 0;
  uint32_t pendingMs = 0;

  int32_t cachedDay = -1;  // epoch day, cached for DAY_CACHE_MS
  unsigned long cachedDayAtMs = 0;
  bool dayCacheValid = false;

  int32_t currentDay(unsigned long nowMs);
  void credit(unsigned long nowMs, unsigned long elapsedMs);
  void addDaySeconds(int32_t epochDay, uint32_t seconds);
  void addBookSeconds(uint32_t hash, uint32_t seconds, uint16_t relDay);
  void parseLine(const char* line, bool& sawTotal);
  int findDay(uint16_t relDay) const;  // binary search; -1 when absent
  int findBook(uint32_t hash) const;   // linear; -1 when absent
};

#define READING_STATS ReadingStats::getInstance()
