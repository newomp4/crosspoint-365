#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>

// Upcoming events for the Calendar sleep screen, fetched from an iCalendar
// feed (e.g. Google Calendar's private "secret address in iCal format"). The
// feed is parsed as it streams in — only occurrences inside the next few days
// are kept, recurring events expanded — and cached on the SD card, so the
// screen can always draw from the last successful fetch.
class CalendarStore : public PersistableStore<CalendarStore> {
  CalendarStore() = default;
  friend class PersistableStore<CalendarStore>;

 public:
  static constexpr size_t URL_LEN = 200;
  static constexpr size_t SUMMARY_LEN = 40;
  static constexpr uint8_t MAX_EVENTS = 20;
  static constexpr uint8_t WINDOW_DAYS = 7;

  struct Event {
    uint32_t startMinute = 0;  // local time, minutes since 1970-01-01 00:00 local
    uint16_t durationMinutes = 0;
    bool allDay = false;
    char summary[SUMMARY_LEN] = "";
    uint32_t endMinute() const { return startMinute + durationMinutes; }
  };

  enum class RefreshResult { Ok, NoUrl, NoWifi, FetchFailed, NotICal };

  static const char* getFilePath() { return "/.crosspoint/calendar.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);
  void ensureLoaded();

  bool hasUrl() const { return url[0] != '\0'; }
  const char* feedUrl() const { return url; }
  void setFeedUrl(const char* text);  // persists; clears the cached events

  bool hasData() const { return fetchedAt != 0; }
  uint32_t fetchedAtEpoch() const { return fetchedAt; }
  uint8_t eventCount() const { return count; }
  const Event& event(const uint8_t i) const { return events[i]; }

  // Blocking fetch + parse; Wi-Fi must be connected. Keeps occurrences that
  // end after `nowLocalMinute` and start within WINDOW_DAYS of it.
  RefreshResult refresh(uint32_t nowEpoch, int32_t utcOffsetMinutes);
  void noteAttempt(uint32_t nowEpoch);
  uint32_t lastAttemptEpoch() const { return lastAttemptAt; }

  // Insertion used by the parser: sorted by start, bounded to MAX_EVENTS.
  void addEvent(const Event& e);

 private:
  char url[URL_LEN] = "";
  Event events[MAX_EVENTS];
  uint8_t count = 0;
  uint32_t fetchedAt = 0;
  uint32_t lastAttemptAt = 0;
  bool loaded = false;
};

#define CALENDAR CalendarStore::getInstance()
