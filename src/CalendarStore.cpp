#include "CalendarStore.h"

#include <CivilDate.h>
#include <Logging.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

#include "network/HttpDownloader.h"

namespace {

// Truncation must not split a UTF-8 sequence: a dangling lead/continuation
// byte at the end would reach the glyph renderer as garbage.
void trimUtf8Tail(char* dest) {
  const size_t len = strlen(dest);
  if (len == 0) return;
  size_t i = len;
  while (i > 0 && (static_cast<uint8_t>(dest[i - 1]) & 0xC0) == 0x80) i--;  // back over continuations
  if (i == 0) {  // nothing but continuation bytes: invalid input, drop it
    dest[0] = '\0';
    return;
  }
  const uint8_t lead = static_cast<uint8_t>(dest[i - 1]);
  if (lead < 0x80) return;  // ASCII tail is complete
  const size_t have = len - i;
  const size_t need = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : 0;
  if (lead < 0xC0 || have < need) dest[i - 1] = '\0';  // stray or cut-off sequence
}

void copyField(char* dest, const char* src, const size_t capacity) {
  if (!src) src = "";
  strncpy(dest, src, capacity - 1);
  dest[capacity - 1] = '\0';
  trimUtf8Tail(dest);
}

// ---- iCalendar streaming parser --------------------------------------------
//
// Lines arrive folded (continuations start with a space or tab); the parser
// unfolds them and keeps one VEVENT's worth of state at a time. Times: "Z"
// values are UTC and shifted by the device offset; TZID values are taken as
// local wall time (the calendar is assumed to live in the device's zone);
// VALUE=DATE marks all-day events. RRULE handles the common shapes:
// FREQ=DAILY/WEEKLY/MONTHLY/YEARLY, INTERVAL, COUNT, UNTIL, BYDAY (weekly),
// and EXDATE exclusions.
class IcsParser {
 public:
  IcsParser(CalendarStore& store, const uint32_t windowStartMinute, const uint32_t windowEndMinute,
            const int32_t utcOffsetMinutes)
      : store(store), windowStart(windowStartMinute), windowEnd(windowEndMinute), offset(utcOffsetMinutes) {}

  void feed(const uint8_t* data, const size_t len) {
    for (size_t i = 0; i < len; i++) {
      const char c = static_cast<char>(data[i]);
      if (c == '\r') continue;
      if (c == '\n') {
        pendingNewline = true;
        continue;
      }
      if (pendingNewline) {
        pendingNewline = false;
        if (c == ' ' || c == '\t') continue;  // folded continuation of the current line
        flushLine();
      }
      if (lineLen < sizeof(line) - 1) line[lineLen++] = c;
    }
  }

  void finish() {
    if (lineLen > 0) flushLine();
  }

 private:
  struct DateTime {
    int32_t minute = -1;  // local epoch minute; -1 = unset
    bool date = false;    // VALUE=DATE
  };

  CalendarStore& store;
  const uint32_t windowStart;
  const uint32_t windowEnd;
  const int32_t offset;

  char line[400];
  size_t lineLen = 0;
  bool pendingNewline = false;

 public:
  // False when the payload never contained a calendar marker — the URL served
  // something else (a login or calendar web page instead of the secret .ics).
  bool sawCalendar = false;

 private:
  bool inEvent = false;
  DateTime dtStart, dtEnd;
  int32_t durationMinutes = -1;
  bool cancelled = false;
  char summary[CalendarStore::SUMMARY_LEN] = "";
  char rrule[120] = "";
  int32_t exdates[10];
  uint8_t exdateCount = 0;

  static int parseInt(const char* s, const int len) {
    int v = 0;
    for (int i = 0; i < len; i++) {
      if (s[i] < '0' || s[i] > '9') return -1;
      v = v * 10 + (s[i] - '0');
    }
    return v;
  }

  // "20260829T140000Z", "20260829T140000", "20260829" -> local epoch minute
  DateTime parseDateTime(const char* value, const bool forceDate) const {
    DateTime dt;
    const int len = static_cast<int>(strlen(value));
    if (len < 8) return dt;
    const int y = parseInt(value, 4), m = parseInt(value + 4, 2), d = parseInt(value + 6, 2);
    if (y < 1970 || m < 1 || m > 12 || d < 1 || d > 31) return dt;
    const int32_t day = CivilDate::daysFromCivil(y, m, d);
    if (forceDate || len == 8) {
      dt.date = true;
      dt.minute = day * 1440;
      return dt;
    }
    if (len < 15 || value[8] != 'T') return dt;
    const int hh = parseInt(value + 9, 2), mm = parseInt(value + 11, 2);
    if (hh < 0 || mm < 0) return dt;
    dt.minute = day * 1440 + hh * 60 + mm;
    if (value[len - 1] == 'Z') dt.minute += offset;
    return dt;
  }

  // "P1D", "PT1H30M", "P2DT3H"
  static int32_t parseDuration(const char* v) {
    int32_t minutes = 0;
    int num = 0;
    bool timePart = false;
    for (const char* p = v; *p; p++) {
      if (*p == 'P') continue;
      if (*p == 'T') {
        timePart = true;
        continue;
      }
      if (*p >= '0' && *p <= '9') {
        num = num * 10 + (*p - '0');
        continue;
      }
      switch (*p) {
        case 'W':
          minutes += num * 7 * 1440;
          break;
        case 'D':
          minutes += num * 1440;
          break;
        case 'H':
          minutes += num * 60;
          break;
        case 'M':
          minutes += timePart ? num : 0;
          break;
        default:
          break;
      }
      num = 0;
    }
    return minutes;
  }

  static void unescapeSummary(const char* src, char* dest, const size_t capacity) {
    size_t n = 0;
    for (const char* p = src; *p && n < capacity - 1; p++) {
      if (*p == '\\' && p[1]) {
        p++;
        if (*p == 'n' || *p == 'N') {
          dest[n++] = ' ';
        } else {
          dest[n++] = *p;
        }
        continue;
      }
      dest[n++] = *p;
    }
    dest[n] = '\0';
    trimUtf8Tail(dest);
  }

  void resetEvent() {
    dtStart = DateTime{};
    dtEnd = DateTime{};
    durationMinutes = -1;
    cancelled = false;
    summary[0] = '\0';
    rrule[0] = '\0';
    exdateCount = 0;
  }

  // Property name up to ':' or ';', parameters, then the value.
  void flushLine() {
    line[lineLen] = '\0';
    lineLen = 0;
    char* colon = strchr(line, ':');
    if (!colon) return;
    *colon = '\0';
    const char* value = colon + 1;
    char* semi = strchr(line, ';');
    const char* params = "";
    if (semi) {
      *semi = '\0';
      params = semi + 1;
    }
    const char* name = line;

    if (strcmp(name, "BEGIN") == 0) {
      if (strcmp(value, "VCALENDAR") == 0) sawCalendar = true;
      if (strcmp(value, "VEVENT") == 0) {
        inEvent = true;
        resetEvent();
      }
      return;
    }
    if (!inEvent) return;
    if (strcmp(name, "END") == 0) {
      if (strcmp(value, "VEVENT") == 0) {
        emitEvent();
        inEvent = false;
      }
      return;
    }
    const bool isDate = strstr(params, "VALUE=DATE") != nullptr && strstr(params, "DATE-TIME") == nullptr;
    if (strcmp(name, "DTSTART") == 0) {
      dtStart = parseDateTime(value, isDate);
    } else if (strcmp(name, "DTEND") == 0) {
      dtEnd = parseDateTime(value, isDate);
    } else if (strcmp(name, "DURATION") == 0) {
      durationMinutes = parseDuration(value);
    } else if (strcmp(name, "SUMMARY") == 0) {
      unescapeSummary(value, summary, sizeof(summary));
    } else if (strcmp(name, "RRULE") == 0) {
      copyField(rrule, value, sizeof(rrule));
    } else if (strcmp(name, "STATUS") == 0) {
      cancelled = strcmp(value, "CANCELLED") == 0;
    } else if (strcmp(name, "EXDATE") == 0) {
      // Comma-separated list, same format as DTSTART.
      const char* p = value;
      while (*p && exdateCount < sizeof(exdates) / sizeof(exdates[0])) {
        char one[20];
        size_t n = 0;
        while (*p && *p != ',' && n < sizeof(one) - 1) one[n++] = *p++;
        one[n] = '\0';
        if (*p == ',') p++;
        const DateTime ex = parseDateTime(one, isDate);
        if (ex.minute >= 0) exdates[exdateCount++] = ex.minute;
      }
    }
  }

  bool excluded(const int32_t startMinute) const {
    for (uint8_t i = 0; i < exdateCount; i++) {
      // Match on the day for all-day exclusions, exact minute otherwise.
      if (exdates[i] == startMinute || exdates[i] / 1440 == startMinute / 1440) return true;
    }
    return false;
  }

  void addOccurrence(const int32_t startMinute, const int32_t duration, const bool allDay) {
    if (excluded(startMinute)) return;
    const uint32_t endMinute = startMinute + std::max<int32_t>(duration, 0);
    if (static_cast<uint32_t>(startMinute) >= windowEnd || endMinute <= windowStart) return;
    CalendarStore::Event e;
    e.startMinute = static_cast<uint32_t>(startMinute);
    e.durationMinutes = static_cast<uint16_t>(std::min<int32_t>(std::max<int32_t>(duration, 0), 0xFFFF));
    e.allDay = allDay;
    copyField(e.summary, summary[0] ? summary : "(untitled)", sizeof(e.summary));
    store.addEvent(e);
  }

  // Value of KEY= inside the RRULE, copied into out (empty when absent).
  void ruleValue(const char* key, char* out, const size_t capacity) const {
    out[0] = '\0';
    const char* p = strstr(rrule, key);
    if (!p || (p != rrule && p[-1] != ';')) return;
    p += strlen(key);
    size_t n = 0;
    while (*p && *p != ';' && n < capacity - 1) out[n++] = *p++;
    out[n] = '\0';
  }

  void emitEvent() {
    if (cancelled || dtStart.minute < 0) return;
    int32_t duration = durationMinutes;
    if (duration < 0) duration = dtEnd.minute >= 0 ? dtEnd.minute - dtStart.minute : (dtStart.date ? 1440 : 0);
    const bool allDay = dtStart.date;

    if (rrule[0] == '\0') {
      addOccurrence(dtStart.minute, duration, allDay);
      return;
    }

    char freq[12], val[24];
    ruleValue("FREQ=", freq, sizeof(freq));
    ruleValue("INTERVAL=", val, sizeof(val));
    const int interval = std::max(1, atoi(val));
    ruleValue("COUNT=", val, sizeof(val));
    int remaining = val[0] ? atoi(val) : -1;
    ruleValue("UNTIL=", val, sizeof(val));
    const DateTime until = val[0] ? parseDateTime(val, false) : DateTime{};
    char byday[40];
    ruleValue("BYDAY=", byday, sizeof(byday));

    // Weekday mask for WEEKLY BYDAY (0 = Sunday), defaulting to DTSTART's day.
    uint8_t dayMask = 0;
    if (byday[0] && strcmp(freq, "WEEKLY") == 0) {
      static constexpr const char* NAMES[7] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
      for (int d = 0; d < 7; d++) {
        if (strstr(byday, NAMES[d])) dayMask |= 1u << d;
      }
    }
    if (dayMask == 0) dayMask = 1u << CivilDate::weekday(dtStart.minute / 1440);

    // Walk occurrences from DTSTART until the window closes. Bounded: a
    // pathological feed cannot stall the wake.
    const int32_t minuteOfDay = allDay ? 0 : dtStart.minute % 1440;
    int32_t day = dtStart.minute / 1440;
    uint16_t startYear;
    uint8_t startMonth, startDom;
    CivilDate::civilFromDays(day, startYear, startMonth, startDom);
    int monthsFromStart = 0;
    for (int guard = 0; guard < 2000; guard++) {
      if (strcmp(freq, "WEEKLY") == 0) {
        // Each week: every masked weekday in the week that holds `day`.
        const int32_t weekStart = day - CivilDate::weekday(day);
        for (int d = 0; d < 7; d++) {
          const int32_t occDay = weekStart + d;
          if (!(dayMask & (1u << d)) || occDay < dtStart.minute / 1440) continue;
          const int32_t occ = occDay * 1440 + minuteOfDay;
          if (until.minute >= 0 && occ > until.minute) return;
          if (remaining == 0) return;
          if (static_cast<uint32_t>(occ) >= windowEnd) return;
          addOccurrence(occ, duration, allDay);
          if (remaining > 0) remaining--;
        }
        day = weekStart + 7 * interval;
        continue;
      }
      const int32_t occ = day * 1440 + minuteOfDay;
      if (until.minute >= 0 && occ > until.minute) return;
      if (remaining == 0) return;
      if (static_cast<uint32_t>(occ) >= windowEnd) return;
      addOccurrence(occ, duration, allDay);
      if (remaining > 0) remaining--;
      if (strcmp(freq, "DAILY") == 0) {
        day += interval;
      } else if (strcmp(freq, "MONTHLY") == 0 || strcmp(freq, "YEARLY") == 0) {
        // Advance to the next month (or year) that has this day-of-month,
        // hopping over months too short for it (a Jan-31 monthly event skips
        // February) without ever emitting a stand-in date.
        const int step = strcmp(freq, "MONTHLY") == 0 ? interval : 12 * interval;
        monthsFromStart += step;
        int hops = 0;
        while (startDom >
               CivilDate::daysInMonth(static_cast<uint16_t>(startYear + (startMonth + monthsFromStart - 1) / 12),
                                      static_cast<uint8_t>((startMonth + monthsFromStart - 1) % 12 + 1))) {
          if (++hops > 60) return;  // >5 years without this date: past any window
          monthsFromStart += step;
        }
        const int y = startYear + (startMonth + monthsFromStart - 1) / 12;
        const int m = (startMonth + monthsFromStart - 1) % 12 + 1;
        day = CivilDate::daysFromCivil(y, m, startDom);
      } else {
        return;  // unsupported FREQ (SECONDLY/MINUTELY/HOURLY): first occurrence only
      }
    }
  }
};

}  // namespace

void CalendarStore::toJson(JsonDocument& doc) const {
  doc["url"] = url;
  doc["f"] = fetchedAt;
  doc["a"] = lastAttemptAt;
  JsonArray arr = doc["ev"].to<JsonArray>();
  for (uint8_t i = 0; i < count; i++) {
    JsonArray e = arr.add<JsonArray>();
    e.add(events[i].startMinute);
    e.add(events[i].durationMinutes);
    e.add(events[i].allDay ? 1 : 0);
    e.add(events[i].summary);
  }
}

bool CalendarStore::fromJson(JsonVariantConst doc) {
  copyField(url, doc["url"] | "", sizeof(url));
  fetchedAt = doc["f"] | 0u;
  lastAttemptAt = doc["a"] | 0u;
  count = 0;
  for (JsonVariantConst e : doc["ev"].as<JsonArrayConst>()) {
    if (count >= MAX_EVENTS) break;
    Event& ev = events[count++];
    ev.startMinute = e[0] | 0u;
    ev.durationMinutes = e[1] | static_cast<uint16_t>(0);
    ev.allDay = (e[2] | 0) != 0;
    copyField(ev.summary, e[3] | "", sizeof(ev.summary));
  }
  return true;
}

void CalendarStore::ensureLoaded() {
  if (loaded) return;
  loaded = true;
  loadFromFile();
}

void CalendarStore::setFeedUrl(const char* text) {
  ensureLoaded();
  // Pasted links often carry stray whitespace; a trailing space breaks the GET.
  while (text && (*text == ' ' || *text == '\n' || *text == '\r' || *text == '\t')) text++;
  copyField(url, text, sizeof(url));
  size_t n = strlen(url);
  while (n > 0 && (url[n - 1] == ' ' || url[n - 1] == '\n' || url[n - 1] == '\r' || url[n - 1] == '\t'))
    url[--n] = '\0';
  count = 0;
  fetchedAt = 0;
  lastAttemptAt = 0;
  saveToFile();
}

void CalendarStore::noteAttempt(const uint32_t nowEpoch) {
  ensureLoaded();
  lastAttemptAt = nowEpoch;
  saveToFile();
}

void CalendarStore::addEvent(const Event& e) {
  // Keep the earliest MAX_EVENTS starts; the screen shows them in order.
  if (count == MAX_EVENTS && e.startMinute >= events[count - 1].startMinute) return;
  int pos = count < MAX_EVENTS ? count : MAX_EVENTS - 1;
  while (pos > 0 && events[pos - 1].startMinute > e.startMinute) {
    events[pos] = events[pos - 1];
    pos--;
  }
  events[pos] = e;
  if (count < MAX_EVENTS) count++;
}

CalendarStore::RefreshResult CalendarStore::refresh(const uint32_t nowEpoch, const int32_t utcOffsetMinutes) {
  ensureLoaded();
  if (!hasUrl()) return RefreshResult::NoUrl;
  if (WiFi.status() != WL_CONNECTED) return RefreshResult::NoWifi;
  lastAttemptAt = nowEpoch;

  const uint32_t nowLocalMinute = nowEpoch / 60 + utcOffsetMinutes;
  const uint32_t windowEnd = (nowLocalMinute / 1440 + WINDOW_DAYS) * 1440;

  // Parse into a scratch table so a failed fetch keeps the previous events.
  Event kept[MAX_EVENTS];
  const uint8_t keptCount = count;
  memcpy(kept, events, sizeof(kept));
  count = 0;

  bool ok = false;
  size_t received = 0;
  bool sawCalendar = false;
  // Two attempts: DNS or a TLS handshake losing the race to a sleepy AP is a
  // common transient, and the next scheduled attempt is far away.
  for (int attempt = 0; attempt < 2 && (!ok || received == 0); ++attempt) {
    if (attempt > 0) {
      LOG_INF("CAL", "Fetch failed; retrying once");
      delay(500);
    }
    count = 0;
    received = 0;
    IcsParser parser(*this, nowLocalMinute, windowEnd, utcOffsetMinutes);
    ok = HttpDownloader::fetchUrl(std::string(url), [&](const uint8_t* data, size_t len) {
      parser.feed(data, len);
      received += len;
      return true;
    });
    parser.finish();
    sawCalendar = parser.sawCalendar;
  }
  if (!ok || received == 0) {
    LOG_ERR("CAL", "Calendar fetch failed (%u bytes)", static_cast<unsigned>(received));
    memcpy(events, kept, sizeof(events));
    count = keptCount;
    saveToFile();
    return RefreshResult::FetchFailed;
  }
  if (!sawCalendar) {
    // The server answered, but not with a calendar: almost always a pasted
    // web-page link instead of the "secret address in iCal format".
    LOG_ERR("CAL", "Response is not an iCal feed (%u bytes)", static_cast<unsigned>(received));
    memcpy(events, kept, sizeof(events));
    count = keptCount;
    saveToFile();
    return RefreshResult::NotICal;
  }
  fetchedAt = nowEpoch;
  saveToFile();
  LOG_INF("CAL", "Calendar: %u events from %u bytes", static_cast<unsigned>(count), static_cast<unsigned>(received));
  return RefreshResult::Ok;
}
