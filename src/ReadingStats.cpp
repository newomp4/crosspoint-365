#include "ReadingStats.h"

#include <CivilDate.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "CrossPointSettings.h"

namespace {
constexpr char STATS_PATH[] = "/.crosspoint/reading_stats.csv";
constexpr char STATS_TMP_PATH[] = "/.crosspoint/reading_stats.tmp";

// Advances *p past the next unsigned number and its trailing comma.
uint32_t nextField(const char*& p) {
  char* end = nullptr;
  const uint32_t v = static_cast<uint32_t>(strtoul(p, &end, 10));
  p = (end && *end == ',') ? end + 1 : (end ? end : p);
  return v;
}
}  // namespace

ReadingStats& ReadingStats::getInstance() {
  static ReadingStats instance;
  return instance;
}

uint32_t ReadingStats::bookHash(const char* path) {
  uint32_t h = 2166136261u;
  for (const char* c = path; c && *c; c++) {
    h ^= static_cast<uint8_t>(*c);
    h *= 16777619u;
  }
  return h == 0 ? 1 : h;
}

void ReadingStats::formatDuration(const uint32_t seconds, char* buf, const size_t bufSize) {
  const unsigned minutes = seconds / 60;
  if (minutes >= 100 * 60) {
    snprintf(buf, bufSize, tr(STR_HM_FMT_HOURS_ONLY), minutes / 60);
  } else if (minutes >= 60) {
    snprintf(buf, bufSize, tr(STR_HM_FMT_HOURS), minutes / 60, minutes % 60);
  } else {
    snprintf(buf, bufSize, tr(STR_HM_FMT_MINUTES), minutes);
  }
}

int ReadingStats::findDay(const uint16_t relDay) const {
  int lo = 0;
  int hi = static_cast<int>(count) - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (days[mid].day == relDay) return mid;
    if (days[mid].day < relDay) {
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return -1;
}

int ReadingStats::findBook(const uint32_t hash) const {
  for (int i = 0; i < bookCount; i++) {
    if (books[i].hash == hash) return i;
  }
  return -1;
}

uint32_t ReadingStats::secondsOn(const int32_t epochDay) {
  if (!loaded) load();
  const int32_t rel = epochDay - DAY_BASE;
  if (rel < 0 || rel > 0xFFFF) return 0;
  const int idx = findDay(static_cast<uint16_t>(rel));
  return idx < 0 ? 0 : days[idx].seconds;
}

uint32_t ReadingStats::allTimeSeconds() {
  if (!loaded) load();
  return allTime;
}

int32_t ReadingStats::firstTrackedDay() {
  if (!loaded) load();
  return count > 0 ? DAY_BASE + days[0].day : -1;
}

uint32_t ReadingStats::secondsForBook(const char* bookPath) {
  if (!loaded) load();
  if (!bookPath || !*bookPath) return 0;
  const int idx = findBook(bookHash(bookPath));
  return idx < 0 ? 0 : books[idx].seconds;
}

void ReadingStats::addDaySeconds(const int32_t epochDay, const uint32_t seconds) {
  const int32_t rel = epochDay - DAY_BASE;
  if (rel < 0 || rel > 0xFFFF || seconds == 0) return;
  const auto relDay = static_cast<uint16_t>(rel);

  const int idx = findDay(relDay);
  if (idx >= 0) {
    days[idx].seconds = static_cast<uint16_t>(std::min<uint32_t>(days[idx].seconds + seconds, 0xFFFF));
    return;
  }
  if (count == MAX_DAYS) {
    // Full: drop the oldest day (the table is sorted ascending).
    memmove(&days[0], &days[1], sizeof(DayEntry) * (MAX_DAYS - 1));
    count--;
  }
  // Insert keeping the sort; new days are almost always the newest, so this
  // usually appends.
  int pos = count;
  while (pos > 0 && days[pos - 1].day > relDay) {
    days[pos] = days[pos - 1];
    pos--;
  }
  days[pos].day = relDay;
  days[pos].seconds = static_cast<uint16_t>(std::min<uint32_t>(seconds, 0xFFFF));
  count++;
}

void ReadingStats::addBookSeconds(const uint32_t hash, const uint32_t seconds, const uint16_t relDay) {
  if (hash == 0 || seconds == 0) return;
  int idx = findBook(hash);
  if (idx < 0) {
    if (bookCount == MAX_BOOKS) {
      // Evict the book that was read least recently.
      idx = 0;
      for (int i = 1; i < bookCount; i++) {
        if (books[i].lastDay < books[idx].lastDay) idx = i;
      }
    } else {
      idx = bookCount++;
    }
    books[idx] = {hash, 0, relDay};
  }
  books[idx].seconds += seconds;
  books[idx].lastDay = std::max(books[idx].lastDay, relDay);
}

ReadingStats::Summary ReadingStats::summarize(const int32_t firstDay, const int32_t lastDay) {
  if (!loaded) load();
  Summary s;
  for (uint16_t i = 0; i < count; i++) {
    const int32_t day = DAY_BASE + days[i].day;
    if (day < firstDay || day > lastDay || days[i].seconds == 0) continue;
    s.totalSeconds += days[i].seconds;
    s.maxSeconds = std::max<uint32_t>(s.maxSeconds, days[i].seconds);
    s.daysRead++;
  }
  // Streak: walk back from lastDay; a blank lastDay (today, nothing read yet)
  // is skipped once so the streak survives until the day is actually over.
  int32_t day = lastDay;
  if (secondsOn(day) == 0) day--;
  while (day >= firstDay - 400 && secondsOn(day) > 0) {
    s.streak++;
    day--;
  }
  return s;
}

int32_t ReadingStats::currentDay(const unsigned long nowMs) {
  if (dayCacheValid && nowMs - cachedDayAtMs < DAY_CACHE_MS) return cachedDay;
  uint16_t year;
  uint8_t month, day;
  cachedDay = halClock.getLocalDate(year, month, day, SETTINGS.clockUtcOffsetQ)
                  ? CivilDate::daysFromCivil(year, month, day)
                  : -1;
  cachedDayAtMs = nowMs;
  dayCacheValid = true;
  return cachedDay;
}

void ReadingStats::credit(const unsigned long nowMs, const unsigned long elapsedMs) {
  pendingMs += elapsedMs;
  const uint32_t seconds = pendingMs / 1000;
  pendingMs %= 1000;
  if (seconds == 0) return;
  const int32_t day = currentDay(nowMs);
  if (day < 0) return;  // no trustworthy date to file it under
  addDaySeconds(day, seconds);
  allTime += seconds;
  const int32_t rel = day - DAY_BASE;
  if (rel >= 0 && rel <= 0xFFFF) addBookSeconds(sessionBook, seconds, static_cast<uint16_t>(rel));
  dirty = true;
}

void ReadingStats::beginSession(const unsigned long nowMs, const char* bookPath) {
  if (!loaded) load();
  inSession = true;
  idleCredited = false;
  lastInputMs = nowMs;
  pendingMs = 0;
  dayCacheValid = false;
  sessionBook = (bookPath && *bookPath) ? bookHash(bookPath) : 0;
}

void ReadingStats::noteInput(const unsigned long nowMs) {
  if (!inSession) return;
  if (!idleCredited) credit(nowMs, std::min(nowMs - lastInputMs, MAX_INPUT_GAP_MS));
  idleCredited = false;
  lastInputMs = nowMs;
}

void ReadingStats::tick(const unsigned long nowMs) {
  if (inSession && !idleCredited && nowMs - lastInputMs >= MAX_INPUT_GAP_MS) {
    // The reader has gone quiet: bank the capped gap now so a dead battery or
    // a crash later doesn't lose it, and don't count anything more until the
    // next input.
    credit(nowMs, MAX_INPUT_GAP_MS);
    idleCredited = true;
  }
  if (dirty && nowMs - lastSaveMs >= SAVE_INTERVAL_MS) {
    save();
    lastSaveMs = nowMs;
  }
}

void ReadingStats::endSession(const unsigned long nowMs) {
  if (!inSession) return;
  if (!idleCredited) credit(nowMs, std::min(nowMs - lastInputMs, MAX_INPUT_GAP_MS));
  inSession = false;
  idleCredited = false;
  sessionBook = 0;
  if (dirty) {
    save();
    lastSaveMs = nowMs;
  }
}

// One CSV record: "t,<allTimeSeconds>", "b,<hash>,<seconds>,<relDay>", or the
// bare "<epochDay>,<seconds>" day line (the original format).
void ReadingStats::parseLine(const char* line, bool& sawTotal) {
  const char* p = line;
  if (*p == 't') {
    p += 2;
    allTime = nextField(p);
    sawTotal = true;
  } else if (*p == 'b') {
    p += 2;
    const uint32_t hash = nextField(p);
    const uint32_t seconds = nextField(p);
    const uint32_t relDay = nextField(p);
    addBookSeconds(hash, seconds, static_cast<uint16_t>(std::min<uint32_t>(relDay, 0xFFFF)));
  } else if (*p >= '0' && *p <= '9') {
    const uint32_t day = nextField(p);
    const uint32_t seconds = nextField(p);
    addDaySeconds(static_cast<int32_t>(day), seconds);
  }
}

bool ReadingStats::load() {
  loaded = true;  // even a missing/corrupt file yields a valid empty table
  count = 0;
  bookCount = 0;
  allTime = 0;
  HalFile file;
  if (!Storage.openFileForRead("RSTAT", STATS_PATH, file)) return false;

  bool sawTotal = false;
  char line[48];
  size_t len = 0;
  uint8_t chunk[64];
  int n;
  while ((n = file.read(chunk, sizeof(chunk))) > 0) {
    for (int i = 0; i < n; i++) {
      const char c = static_cast<char>(chunk[i]);
      if (c == '\n' || c == '\r') {
        if (len > 0) {
          line[len] = '\0';
          parseLine(line, sawTotal);
          len = 0;
        }
      } else if (len < sizeof(line) - 1) {
        line[len++] = c;
      }
    }
  }
  if (len > 0) {
    line[len] = '\0';
    parseLine(line, sawTotal);
  }
  if (!sawTotal) {
    // File from before all-time tracking: the day table is the best estimate.
    for (uint16_t i = 0; i < count; i++) allTime += days[i].seconds;
  }
  dirty = false;
  LOG_INF("RSTAT", "Loaded %u reading days, %u books", static_cast<unsigned>(count), static_cast<unsigned>(bookCount));
  return true;
}

bool ReadingStats::save() {
  {
    HalFile file;
    if (!Storage.openFileForWrite("RSTAT", STATS_TMP_PATH, file)) return false;
    char line[48];
    auto put = [&file, &line](const int len) {
      return len > 0 && file.write(reinterpret_cast<const uint8_t*>(line), len) == static_cast<size_t>(len);
    };
    bool ok = put(snprintf(line, sizeof(line), "t,%lu\n", static_cast<unsigned long>(allTime)));
    for (uint16_t i = 0; ok && i < count; i++) {
      ok = put(snprintf(line, sizeof(line), "%ld,%u\n", static_cast<long>(DAY_BASE + days[i].day),
                        static_cast<unsigned>(days[i].seconds)));
    }
    for (uint8_t i = 0; ok && i < bookCount; i++) {
      ok = put(snprintf(line, sizeof(line), "b,%lu,%lu,%u\n", static_cast<unsigned long>(books[i].hash),
                        static_cast<unsigned long>(books[i].seconds), static_cast<unsigned>(books[i].lastDay)));
    }
    if (!ok) {
      LOG_ERR("RSTAT", "Short write saving reading stats");
      return false;
    }
  }  // closed here so the rename below sees a complete file
  if (Storage.exists(STATS_PATH)) Storage.remove(STATS_PATH);
  if (!Storage.rename(STATS_TMP_PATH, STATS_PATH)) {
    LOG_ERR("RSTAT", "Failed to move reading stats into place");
    return false;
  }
  dirty = false;
  LOG_DBG("RSTAT", "Saved %u reading days, %u books", static_cast<unsigned>(count), static_cast<unsigned>(bookCount));
  return true;
}
