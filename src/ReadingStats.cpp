#include "ReadingStats.h"

#include <CivilDate.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"

namespace {
constexpr char STATS_PATH[] = "/.crosspoint/reading_stats.csv";
constexpr char STATS_TMP_PATH[] = "/.crosspoint/reading_stats.tmp";
}  // namespace

ReadingStats& ReadingStats::getInstance() {
  static ReadingStats instance;
  return instance;
}

int ReadingStats::findIndex(const uint16_t relDay) const {
  int lo = 0;
  int hi = static_cast<int>(count) - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (entries[mid].day == relDay) return mid;
    if (entries[mid].day < relDay) {
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return -1;
}

uint32_t ReadingStats::secondsOn(const int32_t epochDay) {
  if (!loaded) load();
  const int32_t rel = epochDay - DAY_BASE;
  if (rel < 0 || rel > 0xFFFF) return 0;
  const int idx = findIndex(static_cast<uint16_t>(rel));
  return idx < 0 ? 0 : entries[idx].seconds;
}

void ReadingStats::addSeconds(const int32_t epochDay, const uint32_t seconds) {
  if (!loaded) load();
  const int32_t rel = epochDay - DAY_BASE;
  if (rel < 0 || rel > 0xFFFF || seconds == 0) return;
  const auto relDay = static_cast<uint16_t>(rel);

  const int idx = findIndex(relDay);
  if (idx >= 0) {
    const uint32_t sum = entries[idx].seconds + seconds;
    entries[idx].seconds = static_cast<uint16_t>(std::min<uint32_t>(sum, 0xFFFF));
    dirty = true;
    return;
  }

  if (count == MAX_DAYS) {
    // Full: drop the oldest day (the table is sorted ascending).
    memmove(&entries[0], &entries[1], sizeof(Entry) * (MAX_DAYS - 1));
    count--;
  }
  // Insert keeping the sort; new days are almost always the newest, so this
  // usually appends.
  int pos = count;
  while (pos > 0 && entries[pos - 1].day > relDay) {
    entries[pos] = entries[pos - 1];
    pos--;
  }
  entries[pos].day = relDay;
  entries[pos].seconds = static_cast<uint16_t>(std::min<uint32_t>(seconds, 0xFFFF));
  count++;
  dirty = true;
}

ReadingStats::Summary ReadingStats::summarize(const int32_t firstDay, const int32_t lastDay) {
  if (!loaded) load();
  Summary s;
  for (uint16_t i = 0; i < count; i++) {
    const int32_t day = DAY_BASE + entries[i].day;
    if (day < firstDay || day > lastDay || entries[i].seconds == 0) continue;
    s.totalSeconds += entries[i].seconds;
    s.maxSeconds = std::max<uint32_t>(s.maxSeconds, entries[i].seconds);
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
  addSeconds(day, seconds);
}

void ReadingStats::beginSession(const unsigned long nowMs) {
  if (!loaded) load();
  inSession = true;
  idleCredited = false;
  lastInputMs = nowMs;
  pendingMs = 0;
  dayCacheValid = false;
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
  if (dirty) {
    save();
    lastSaveMs = nowMs;
  }
}

bool ReadingStats::load() {
  loaded = true;  // even a missing/corrupt file yields a valid empty table
  count = 0;
  HalFile file;
  if (!Storage.openFileForRead("RSTAT", STATS_PATH, file)) return false;

  // Lines of "epochDay,seconds\n"; parsed byte by byte so no line buffer is needed.
  uint32_t value = 0;
  uint32_t dayField = 0;
  bool inSeconds = false;
  bool hasDigits = false;
  uint8_t chunk[64];
  int n;
  while ((n = file.read(chunk, sizeof(chunk))) > 0) {
    for (int i = 0; i < n; i++) {
      const char c = static_cast<char>(chunk[i]);
      if (c >= '0' && c <= '9') {
        value = value * 10 + static_cast<uint32_t>(c - '0');
        hasDigits = true;
      } else if (c == ',') {
        dayField = value;
        value = 0;
        inSeconds = true;
      } else if (c == '\n' || c == '\r') {
        if (inSeconds && hasDigits) addSeconds(static_cast<int32_t>(dayField), value);
        value = 0;
        inSeconds = false;
        hasDigits = false;
      }
    }
  }
  if (inSeconds && hasDigits) addSeconds(static_cast<int32_t>(dayField), value);
  dirty = false;
  LOG_INF("RSTAT", "Loaded %u reading days", static_cast<unsigned>(count));
  return true;
}

bool ReadingStats::save() {
  {
    HalFile file;
    if (!Storage.openFileForWrite("RSTAT", STATS_TMP_PATH, file)) return false;
    char line[24];
    for (uint16_t i = 0; i < count; i++) {
      const int len = snprintf(line, sizeof(line), "%ld,%u\n", static_cast<long>(DAY_BASE + entries[i].day),
                               static_cast<unsigned>(entries[i].seconds));
      if (file.write(reinterpret_cast<const uint8_t*>(line), len) != static_cast<size_t>(len)) {
        LOG_ERR("RSTAT", "Short write saving reading stats");
        return false;
      }
    }
  }  // closed here so the rename below sees a complete file
  if (Storage.exists(STATS_PATH)) Storage.remove(STATS_PATH);
  if (!Storage.rename(STATS_TMP_PATH, STATS_PATH)) {
    LOG_ERR("RSTAT", "Failed to move reading stats into place");
    return false;
  }
  dirty = false;
  LOG_DBG("RSTAT", "Saved %u reading days", static_cast<unsigned>(count));
  return true;
}
