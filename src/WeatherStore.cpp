#include "WeatherStore.h"

#include <Logging.h>
#include <WiFi.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "CrossPointSettings.h"
#include "network/HttpDownloader.h"

namespace {
constexpr char GEOCODE_URL[] = "https://geocoding-api.open-meteo.com/v1/search?count=1&language=en&format=json&name=";
constexpr char FORECAST_URL[] =
    "https://api.open-meteo.com/v1/forecast?current=temperature_2m,weather_code,is_day"
    "&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1";

void copyField(char* dest, const char* src, const size_t capacity) {
  if (!src) src = "";
  strncpy(dest, src, capacity - 1);
  dest[capacity - 1] = '\0';
}

// Percent-encodes everything but unreserved characters.
void appendUrlEncoded(std::string& url, const char* text) {
  static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
  for (const char* c = text; *c; c++) {
    const auto ch = static_cast<unsigned char>(*c);
    if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
      url += static_cast<char>(ch);
    } else {
      url += '%';
      url += HEX_DIGITS[ch >> 4];
      url += HEX_DIGITS[ch & 0x0F];
    }
  }
}

int16_t toTenths(const float celsius) { return static_cast<int16_t>(lroundf(celsius * 10.0f)); }

// Verified https first; Open-Meteo also serves plain http, which is the
// fallback when the TLS handshake fails (typically heap pressure).
bool fetch(const std::string& httpsUrl, std::string& body) {
  if (HttpDownloader::fetchUrl(httpsUrl, body)) return true;
  body.clear();
  const std::string httpUrl = "http://" + httpsUrl.substr(sizeof("https://") - 1);
  LOG_INF("WX", "https fetch failed, retrying over http");
  return HttpDownloader::fetchUrl(httpUrl, body);
}
}  // namespace

void WeatherStore::toJson(JsonDocument& doc) const {
  doc["q"] = query;
  doc["rq"] = resolvedQuery;
  doc["n"] = name;
  doc["lat"] = lat;
  doc["lon"] = lon;
  doc["t"] = tempC10;
  doc["hi"] = maxC10_;
  doc["lo"] = minC10_;
  doc["c"] = code;
  doc["d"] = day;
  doc["f"] = fetchedAt;
  doc["a"] = lastAttemptAt;
}

bool WeatherStore::fromJson(JsonVariantConst doc) {
  copyField(query, doc["q"] | "", sizeof(query));
  copyField(resolvedQuery, doc["rq"] | "", sizeof(resolvedQuery));
  copyField(name, doc["n"] | "", sizeof(name));
  lat = doc["lat"] | 0.0f;
  lon = doc["lon"] | 0.0f;
  tempC10 = doc["t"] | static_cast<int16_t>(0);
  maxC10_ = doc["hi"] | static_cast<int16_t>(0);
  minC10_ = doc["lo"] | static_cast<int16_t>(0);
  code = doc["c"] | static_cast<uint8_t>(0);
  day = doc["d"] | true;
  fetchedAt = doc["f"] | 0u;
  lastAttemptAt = doc["a"] | 0u;
  return true;
}

void WeatherStore::ensureLoaded() {
  if (loaded) return;
  loaded = true;
  loadFromFile();
}

void WeatherStore::setLocationQuery(const char* text) {
  ensureLoaded();
  while (text && (*text == ' ' || *text == '\n' || *text == '\t')) text++;
  copyField(query, text, sizeof(query));
  size_t n = strlen(query);
  while (n > 0 && (query[n - 1] == ' ' || query[n - 1] == '\n' || query[n - 1] == '\t')) query[--n] = '\0';
  if (strcmp(query, resolvedQuery) != 0) {
    // New place: forget the old coordinates and conditions.
    resolvedQuery[0] = '\0';
    name[0] = '\0';
    fetchedAt = 0;
    lastAttemptAt = 0;
  }
  saveToFile();
}

bool WeatherStore::isStale(const uint32_t nowEpoch, const uint32_t maxAgeSeconds) const {
  if (fetchedAt == 0) return true;
  if (nowEpoch < fetchedAt) return true;  // clock moved backwards: treat as unknown
  return nowEpoch - fetchedAt > maxAgeSeconds;
}

bool WeatherStore::shouldAutoRefresh(const uint32_t nowEpoch) const {
  if (!hasLocation() || nowEpoch == 0) return false;
  if (!isStale(nowEpoch, AUTO_MAX_AGE_S)) return false;
  return lastAttemptAt == 0 || nowEpoch < lastAttemptAt || nowEpoch - lastAttemptAt > AUTO_MAX_AGE_S;
}

void WeatherStore::noteAttempt(const uint32_t nowEpoch) {
  ensureLoaded();
  lastAttemptAt = nowEpoch;
  saveToFile();
}

void WeatherStore::refreshIfDue(const uint32_t nowEpoch) {
  ensureLoaded();
  if (!hasLocation() || !isStale(nowEpoch, OPPORTUNISTIC_MAX_AGE_S)) return;
  refresh(nowEpoch);
}

bool WeatherStore::geocode() {
  std::string url = GEOCODE_URL;
  appendUrlEncoded(url, query);
  std::string body;
  if (!fetch(url, body)) {
    LOG_ERR("WX", "Geocoding request failed");
    return false;
  }
  JsonDocument filter;
  filter["results"][0]["name"] = true;
  filter["results"][0]["latitude"] = true;
  filter["results"][0]["longitude"] = true;
  filter["results"][0]["country_code"] = true;
  JsonDocument doc;
  const auto err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (err || doc["results"][0]["latitude"].isNull()) {
    LOG_ERR("WX", "No geocoding match for '%s'", query);
    return false;
  }
  lat = doc["results"][0]["latitude"] | 0.0f;
  lon = doc["results"][0]["longitude"] | 0.0f;
  const char* placeName = doc["results"][0]["name"] | "";
  const char* country = doc["results"][0]["country_code"] | "";
  if (country[0]) {
    snprintf(name, sizeof(name), "%s, %s", placeName, country);
  } else {
    copyField(name, placeName, sizeof(name));
  }
  copyField(resolvedQuery, query, sizeof(resolvedQuery));
  LOG_INF("WX", "Resolved '%s' to %s (%.3f, %.3f)", query, name, static_cast<double>(lat), static_cast<double>(lon));
  return true;
}

bool WeatherStore::fetchForecast() {
  char coords[64];
  snprintf(coords, sizeof(coords), "&latitude=%.4f&longitude=%.4f", static_cast<double>(lat), static_cast<double>(lon));
  std::string url = FORECAST_URL;
  url += coords;
  std::string body;
  if (!fetch(url, body)) {
    LOG_ERR("WX", "Forecast request failed");
    return false;
  }
  JsonDocument filter;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["weather_code"] = true;
  filter["current"]["is_day"] = true;
  filter["daily"]["temperature_2m_max"][0] = true;
  filter["daily"]["temperature_2m_min"][0] = true;
  filter["utc_offset_seconds"] = true;
  JsonDocument doc;
  const auto err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (err || doc["current"]["temperature_2m"].isNull()) {
    LOG_ERR("WX", "Forecast response not understood");
    return false;
  }
  // timezone=auto makes Open-Meteo resolve the city's zone; adopting its UTC
  // offset fixes the device clock's local time (and DST shifts) without the
  // user ever finding the quarter-hour offset setting.
  if (!doc["utc_offset_seconds"].isNull()) {
    const int32_t offsetSeconds = doc["utc_offset_seconds"] | 0;
    const int32_t q = 48 + offsetSeconds / 900;
    if (q >= 0 && q <= 104 && static_cast<uint8_t>(q) != SETTINGS.clockUtcOffsetQ) {
      LOG_INF("WX", "Adopting UTC offset from weather location: %+d min", static_cast<int>(offsetSeconds / 60));
      SETTINGS.clockUtcOffsetQ = static_cast<uint8_t>(q);
      SETTINGS.saveToFile();
    }
  }
  tempC10 = toTenths(doc["current"]["temperature_2m"] | 0.0f);
  code = doc["current"]["weather_code"] | static_cast<uint8_t>(0);
  day = (doc["current"]["is_day"] | 1) != 0;
  maxC10_ = toTenths(doc["daily"]["temperature_2m_max"][0] | 0.0f);
  minC10_ = toTenths(doc["daily"]["temperature_2m_min"][0] | 0.0f);
  return true;
}

WeatherStore::RefreshResult WeatherStore::refresh(const uint32_t nowEpoch) {
  ensureLoaded();
  if (!hasLocation()) return RefreshResult::NoLocation;
  if (WiFi.status() != WL_CONNECTED) return RefreshResult::NoWifi;
  lastAttemptAt = nowEpoch;

  if (resolvedQuery[0] == '\0' || strcmp(resolvedQuery, query) != 0) {
    if (!geocode()) {
      saveToFile();
      return RefreshResult::GeocodeFailed;
    }
  }
  if (!fetchForecast()) {
    saveToFile();
    return RefreshResult::FetchFailed;
  }
  fetchedAt = nowEpoch ? nowEpoch : 1;  // 1 = fetched, age unknown (no RTC)
  saveToFile();
  LOG_INF("WX", "%s: %d.%d C, code %u, %s", name, tempC10 / 10, abs(tempC10 % 10), code, day ? "day" : "night");
  return RefreshResult::Ok;
}

void WeatherStore::formatTemperature(const int16_t c10, const uint8_t unit, char* buf, const size_t bufSize) {
  // Round to whole degrees in the display unit; U+00B0 is in every UI font used here.
  const float celsius = c10 / 10.0f;
  const long value = lroundf(unit == 1 ? celsius * 9.0f / 5.0f + 32.0f : celsius);
  snprintf(buf, bufSize, "%ld\xC2\xB0", value);
}

StrId WeatherStore::conditionName(const uint8_t wmoCode) {
  if (wmoCode == 0) return StrId::STR_WX_CLEAR;
  if (wmoCode <= 2) return StrId::STR_WX_PARTLY;
  if (wmoCode == 3) return StrId::STR_WX_CLOUDY;
  if (wmoCode == 45 || wmoCode == 48) return StrId::STR_WX_FOG;
  if (wmoCode >= 51 && wmoCode <= 57) return StrId::STR_WX_DRIZZLE;
  if (wmoCode >= 61 && wmoCode <= 67) return StrId::STR_WX_RAIN;
  if (wmoCode >= 71 && wmoCode <= 77) return StrId::STR_WX_SNOW;
  if (wmoCode >= 80 && wmoCode <= 82) return StrId::STR_WX_SHOWERS;
  if (wmoCode == 85 || wmoCode == 86) return StrId::STR_WX_SNOW;
  if (wmoCode >= 95) return StrId::STR_WX_THUNDER;
  return StrId::STR_WX_CLOUDY;
}
