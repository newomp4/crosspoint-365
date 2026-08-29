#pragma once
#include <ArduinoJson.h>
#include <I18n.h>
#include <PersistableStore.h>

#include <cstdint>

// Weather for the home-screen widget, from Open-Meteo (no API key). Holds the
// user's location (a typed place name, resolved to coordinates by the
// geocoding API on the first refresh) and the last fetched conditions, so the
// widget never needs the radio: refresh() runs only while Wi-Fi is already
// connected — opportunistically from the Wi-Fi flow, or from the explicit
// "Refresh weather now" action.
class WeatherStore : public PersistableStore<WeatherStore> {
  WeatherStore() = default;
  friend class PersistableStore<WeatherStore>;

 public:
  static constexpr size_t LOCATION_LEN = 40;
  // Opportunistic refresh (radio already up) once the data is this old.
  static constexpr uint32_t OPPORTUNISTIC_MAX_AGE_S = 60u * 60u;
  // Auto-refresh on wake brings the radio up, so it waits longer, and a
  // failed attempt is not retried for the same interval.
  static constexpr uint32_t AUTO_MAX_AGE_S = 3u * 60u * 60u;

  enum class RefreshResult { Ok, NoLocation, NoWifi, GeocodeFailed, FetchFailed };

  static const char* getFilePath() { return "/.crosspoint/weather.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);
  void ensureLoaded();

  bool hasLocation() const { return query[0] != '\0'; }
  const char* locationQuery() const { return query; }
  // Resolved place name when known, else what the user typed.
  const char* locationName() const { return name[0] ? name : query; }
  void setLocationQuery(const char* text);  // persists; clears cached data

  bool hasData() const { return fetchedAt != 0; }
  uint32_t fetchedAtEpoch() const { return fetchedAt; }
  bool isStale(uint32_t nowEpoch, uint32_t maxAgeSeconds) const;
  // Location set, data older than AUTO_MAX_AGE_S, and no attempt within that window.
  bool shouldAutoRefresh(uint32_t nowEpoch) const;

  // Blocking fetch; Wi-Fi must be connected. Marks the attempt time on any outcome.
  RefreshResult refresh(uint32_t nowEpoch);
  // Record an attempt that never reached the network (no Wi-Fi), for the throttle.
  void noteAttempt(uint32_t nowEpoch);
  // refresh() only when a location is set and the data is past OPPORTUNISTIC_MAX_AGE_S.
  void refreshIfDue(uint32_t nowEpoch);

  int16_t temperatureC10() const { return tempC10; }
  int16_t maxC10() const { return maxC10_; }
  int16_t minC10() const { return minC10_; }
  uint8_t weatherCode() const { return code; }
  bool isDaytime() const { return day; }

  // "21°" in the requested unit (CrossPointSettings::WEATHER_UNIT).
  static void formatTemperature(int16_t c10, uint8_t unit, char* buf, size_t bufSize);
  static StrId conditionName(uint8_t wmoCode);

 private:
  char query[LOCATION_LEN] = "";
  char resolvedQuery[LOCATION_LEN] = "";  // query the coordinates belong to
  char name[LOCATION_LEN] = "";
  float lat = 0;
  float lon = 0;
  int16_t tempC10 = 0;
  int16_t maxC10_ = 0;
  int16_t minC10_ = 0;
  uint8_t code = 0;
  bool day = true;
  uint32_t fetchedAt = 0;
  uint32_t lastAttemptAt = 0;
  bool loaded = false;

  bool geocode();
  bool fetchForecast();
};

#define WEATHER WeatherStore::getInstance()
